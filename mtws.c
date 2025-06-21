#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char **buf;
    int capacity, count, head, tail;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty, not_full;
} buffer_t;

static buffer_t buffer;
static int producers_done = 0;
static pthread_mutex_t total_mtx = PTHREAD_MUTEX_INITIALIZER;
static int total_count = 0;
static char *search_word = NULL;

// 도움말 출력 매크로
#define USAGE_MSG() do { \
    fprintf(stderr, "Usage: %s -b <buffer size> -t <num threads> -d <directory> -w <word>\n", argv[0]); \
    fprintf(stderr, "  -b : bounded buffer size\n"); \
    fprintf(stderr, "  -t : number of threads searching word (except for main thread)\n"); \
    fprintf(stderr, "  -d : search directory (includes subdirectories)\n"); \
    fprintf(stderr, "  -w : search word (case-insensitive)\n"); \
} while(0)

// 프로토타입
void to_lower_str(char *s);
int is_text_file(const char *name);
void *worker(void *arg);

// 버퍼 초기화
void buf_init(buffer_t *b, int capacity) {
    b->capacity = capacity;
    b->count = b->head = b->tail = 0;
    b->buf = malloc(sizeof(char*) * capacity);
    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
}

// 버퍼 해제
void buf_destroy(buffer_t *b) {
    free(b->buf);
    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->not_empty);
    pthread_cond_destroy(&b->not_full);
}

// push (block if full)
void buf_push(buffer_t *b, char *item) {
    pthread_mutex_lock(&b->mtx);
    while (b->count == b->capacity)
        pthread_cond_wait(&b->not_full, &b->mtx);
    b->buf[b->tail] = item;
    b->tail = (b->tail + 1) % b->capacity;
    b->count++;
    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->mtx);
}

// pop (block if empty; NULL on completion)
char *buf_pop(buffer_t *b) {
    pthread_mutex_lock(&b->mtx);
    while (b->count == 0 && !producers_done)
        pthread_cond_wait(&b->not_empty, &b->mtx);
    if (b->count == 0 && producers_done) {
        pthread_mutex_unlock(&b->mtx);
        return NULL;
    }
    char *item = b->buf[b->head];
    b->head = (b->head + 1) % b->capacity;
    b->count--;
    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->mtx);
    return item;
}

// 문자열을 소문자로
void to_lower_str(char *s) {
    for (; *s; ++s) *s = tolower((unsigned char)*s);
}

// 텍스트 파일 필터링(.txt, .c, .h)
int is_text_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return strcmp(ext, ".txt")==0
        || strcmp(ext, ".c")==0
        || strcmp(ext, ".h")==0;
}

// 디렉터리 재귀 탐색 → buf_push
void traverse_dir(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0]=='.') continue;  // 숨김 건너뛰기
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(path, &st)==0) {
            if (S_ISDIR(st.st_mode))
                traverse_dir(path);
            else if (S_ISREG(st.st_mode) && is_text_file(entry->d_name))
                buf_push(&buffer, strdup(path));
        }
    }
    closedir(dir);
}

// 워커 스레드: buf_pop → 검색 → 개수 합산 → 출력
void *worker(void *arg) {
    (void)arg;
    while (1) {
        char *path = buf_pop(&buffer);
        if (!path) break;
        FILE *fp = fopen(path, "r");
        if (fp) {
            size_t len = 0;
            char *line = NULL;
            int local_count = 0;
            char *lower_word = strdup(search_word);
            to_lower_str(lower_word);
            while (getline(&line, &len, fp) != -1) {
                char *dup = strdup(line);
                to_lower_str(dup);
                char *p = dup;
                while ((p = strstr(p, lower_word)) != NULL) {
                    local_count++;
                    p += strlen(lower_word);
                }
                free(dup);
            }
            free(line);
            free(lower_word);
            fclose(fp);
            printf("[Thread %lu] %s: %d found\n",
                   (unsigned long)pthread_self(), path, local_count);
            pthread_mutex_lock(&total_mtx);
            total_count += local_count;
            pthread_mutex_unlock(&total_mtx);
        }
        free(path);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int bufsize=0, num_threads=0;
    char *dirpath=NULL;
    int opt;

    // 옵션 파싱
    while ((opt = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (opt) {
            case 'b': bufsize     = atoi(optarg);   break;
            case 't': num_threads = atoi(optarg);   break;
            case 'd': dirpath     = strdup(optarg); break;
            case 'w': search_word = strdup(optarg); break;
            default:
                USAGE_MSG();
                exit(EXIT_FAILURE);
        }
    }
    if (!bufsize || !num_threads || !dirpath || !search_word) {
        USAGE_MSG();
        exit(EXIT_FAILURE);
    }

    buf_init(&buffer, bufsize);

    // 워커 스레드 생성
    pthread_t *threads = malloc(sizeof(pthread_t)*num_threads);
    for (int i=0; i<num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL)!=0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // 메인 스레드가 디렉터리 탐색
    traverse_dir(dirpath);

    // 종료 신호
    pthread_mutex_lock(&buffer.mtx);
    producers_done = 1;
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_mutex_unlock(&buffer.mtx);

    // 워커 종료 대기
    for (int i=0; i<num_threads; i++)
        pthread_join(threads[i], NULL);

    // 최종 합계 출력
    printf("Total found = %d\n", total_count);

    buf_destroy(&buffer);
    free(dirpath);
    free(search_word);
    free(threads);
    return 0;
}
