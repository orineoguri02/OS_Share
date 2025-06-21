/*
 * mtws.c
 * Multi-threaded word search with bounded buffer and synchronization
 * Usage: ./mtws -b <buffer size> -t <num threads> -d <directory> -w <word>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// Bounded buffer structure
typedef struct {
    char **buf;
    int capacity;
    int count;
    int head;
    int tail;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} buffer_t;

static buffer_t buffer;
static int num_threads;
static char *search_word;
static int total_count = 0;
static pthread_mutex_t total_mtx = PTHREAD_MUTEX_INITIALIZER;
static int producers_done = 0;

// Initialize buffer
void buf_init(buffer_t *b, int capacity) {
    b->capacity = capacity;
    b->count = b->head = b->tail = 0;
    b->buf = malloc(sizeof(char*) * capacity);
    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->not_empty, NULL);
    pthread_cond_init(&b->not_full, NULL);
}

// Destroy buffer
void buf_destroy(buffer_t *b) {
    free(b->buf);
    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->not_empty);
    pthread_cond_destroy(&b->not_full);
}

// Push a path into buffer (blocks if full)
void buf_push(buffer_t *b, char *path) {
    pthread_mutex_lock(&b->mtx);
    while (b->count == b->capacity)
        pthread_cond_wait(&b->not_full, &b->mtx);
    b->buf[b->tail] = path;
    b->tail = (b->tail + 1) % b->capacity;
    b->count++;
    pthread_cond_signal(&b->not_empty);
    pthread_mutex_unlock(&b->mtx);
}

// Pop a path from buffer (blocks if empty)
char *buf_pop(buffer_t *b) {
    pthread_mutex_lock(&b->mtx);
    while (b->count == 0 && !producers_done)
        pthread_cond_wait(&b->not_empty, &b->mtx);
    if (b->count == 0 && producers_done) {
        pthread_mutex_unlock(&b->mtx);
        return NULL;
    }
    char *path = b->buf[b->head];
    b->head = (b->head + 1) % b->capacity;
    b->count--;
    pthread_cond_signal(&b->not_full);
    pthread_mutex_unlock(&b->mtx);
    return path;
}

// Recursively traverse directory and push file paths
void traverse_dir(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    struct dirent *entry;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                traverse_dir(fullpath);
            } else if (S_ISREG(st.st_mode)) {
                buf_push(&buffer, strdup(fullpath));
            }
        }
    }
    closedir(dir);
}

// Convert string to lowercase in-place\ void to_lower_str(char *s) {
    for (; *s; ++s) *s = tolower(*s);
}

// Worker thread function
void *worker(void *arg) {
    while (1) {
        char *path = buf_pop(&buffer);
        if (!path) break;  // no more files
        FILE *fp = fopen(path, "r");
        if (!fp) {
            perror("fopen");
            free(path);
            continue;
        }
        int count = 0;
        char *line = NULL;
        size_t len = 0;
        char *lcopy, *p;
        while (getline(&line, &len, fp) != -1) {
            lcopy = strdup(line);
            to_lower_str(lcopy);
            p = lcopy;
            while ((p = strstr(p, search_word)) != NULL) {
                count++;
                p += strlen(search_word);
            }
            free(lcopy);
        }
        free(line);
        fclose(fp);
        printf("[%ld] %s: %d occurrences\n", (long)pthread_self(), path, count);
        pthread_mutex_lock(&total_mtx);
        total_count += count;
        pthread_mutex_unlock(&total_mtx);
        free(path);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int bufsize = 0;
    char *dirpath = NULL;
    int c;
    while ((c = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (c) {
            case 'b': bufsize = atoi(optarg); break;
            case 't': num_threads = atoi(optarg); break;
            case 'd': dirpath = strdup(optarg); break;
            case 'w':
                search_word = strdup(optarg);
                to_lower_str(search_word);
                break;
            default:
                fprintf(stderr, "Usage: %s -b <bufsize> -t <threads> -d <dir> -w <word>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (!bufsize || !num_threads || !dirpath || !search_word) {
        fprintf(stderr, "Missing arguments.\n");
        exit(EXIT_FAILURE);
    }

    buf_init(&buffer, bufsize);

    // Create worker threads
    pthread_t workers[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&workers[i], NULL, worker, NULL);
    }

    // Producer: traverse directory and push files
    traverse_dir(dirpath);

    // Signal no more files
    pthread_mutex_lock(&buffer.mtx);
    producers_done = 1;
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_mutex_unlock(&buffer.mtx);

    // Join workers
    for (int i = 0; i < num_threads; i++) {
        pthread_join(workers[i], NULL);
    }

    printf("Total occurrences of '%s': %d\n", search_word, total_count);

    buf_destroy(&buffer);
    free(dirpath);
    free(search_word);
    return 0;
}
