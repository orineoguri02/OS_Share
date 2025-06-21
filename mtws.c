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

// helper prototypes
void to_lower_str(char *s);
int is_text_file(const char *name);
void *worker(void *arg);

// initialize bounded buffer
void buf_init(buffer_t *b, int capacity) {
    b->capacity = capacity;
    b->count = b->head = b->tail = 0;
    b->buf = malloc(sizeof(char*) * capacity);
    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
}

// destroy buffer
void buf_destroy(buffer_t *b) {
    free(b->buf);
    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->not_empty);
    pthread_cond_destroy(&b->not_full);
}

// push an item into the buffer (blocks if full)
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

// pop an item from the buffer (blocks if empty)
// returns NULL when no more items will arrive
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

// convert string in-place to lowercase
void to_lower_str(char *s) {
    for (; *s; ++s) *s = tolower((unsigned char)*s);
}

// filter: only .txt, .c, .h files
int is_text_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return strcmp(ext, ".txt") == 0
        || strcmp(ext, ".c")   == 0
        || strcmp(ext, ".h")   == 0;
}

// recursively traverse directories, push each matching file path
void traverse_dir(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // skip hidden files/dirs
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                traverse_dir(path);
            } else if (S_ISREG(st.st_mode) && is_text_file(entry->d_name)) {
                buf_push(&buffer, strdup(path));
            }
        }
    }
    closedir(dir);
}

// worker thread: pop paths, search word, report count
void *worker(void *arg) {
    (void)arg;
    while (1) {
        char *path = buf_pop(&buffer);
        if (!path) break;
        FILE *fp = fopen(path, "r");
        if (!fp) {
            free(path);
            continue;
        }
        size_t len = 0;
        char *line = NULL;
        int local_count = 0;
        char *lower_word = strdup(search_word);
        to_lower_str(lower_word);

        while (getline(&line, &len, fp) != -1) {
            char *lcopy = strdup(line);
            to_lower_str(lcopy);
            char *p = lcopy;
            while ((p = strstr(p, lower_word)) != NULL) {
                local_count++;
                p += strlen(lower_word);
            }
            free(lcopy);
        }

        free(lower_word);
        free(line);
        fclose(fp);

        printf("[thread %lu] %s: %d found\n",
               (unsigned long)pthread_self(), path, local_count);

        pthread_mutex_lock(&total_mtx);
        total_count += local_count;
        pthread_mutex_unlock(&total_mtx);

        free(path);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int bufsize = 0, num_threads = 0;
    char *dirpath = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (opt) {
        case 'b': bufsize      = atoi(optarg);   break;
        case 't': num_threads  = atoi(optarg);   break;
        case 'd': dirpath      = strdup(optarg); break;
        case 'w': search_word  = strdup(optarg); break;
        default:
            fprintf(stderr, "Usage: %s -b <bufsize> -t <threads> -d <dir> -w <word>\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (!bufsize || !num_threads || !dirpath || !search_word) {
        fprintf(stderr, "Missing required argument\n");
        exit(EXIT_FAILURE);
    }

    buf_init(&buffer, bufsize);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    traverse_dir(dirpath);

    // signal consumers that production is done
    pthread_mutex_lock(&buffer.mtx);
    producers_done = 1;
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_mutex_unlock(&buffer.mtx);

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    printf("Total found = %d\n", total_count);

    buf_destroy(&buffer);
    free(dirpath);
    free(search_word);
    free(threads);
    return 0;
}
