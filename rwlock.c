#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define MAX_THREADS 100

sem_t mutex;
sem_t queue;
sem_t rw_mutex;
int readcount = 0;
char types[MAX_THREADS];
int times_ms[MAX_THREADS];
int thread_count = 0;

void init_lock() {
    sem_init(&mutex, 0, 1);
    sem_init(&queue, 0, 1);
    sem_init(&rw_mutex, 0, 1);
}

void acquire_read() {
    sem_wait(&queue);
    sem_wait(&mutex);
    readcount++;
    if (readcount == 1) sem_wait(&rw_mutex);
    sem_post(&mutex);
    sem_post(&queue);
}

void release_read() {
    sem_wait(&mutex);
    readcount--;
    if (readcount == 0) sem_post(&rw_mutex);
    sem_post(&mutex);
}

void acquire_write() {
    sem_wait(&queue);
    sem_wait(&rw_mutex);
    sem_post(&queue);
}

void release_write() {
    sem_post(&rw_mutex);
}

void* thread_func(void* arg) {
    int id = (int)(long)arg;
    char type = types[id];
    int ms = times_ms[id];
    if (type == 'R') {
        printf("[Created] Reader#%d\n", id+1);
        acquire_read();
        printf("[Start]   Reader#%d 읽기 시작\n", id+1);
        usleep(ms * 1000);
        printf("[Finish]  Reader#%d 읽기 종료\n", id+1);
        release_read();
    } else {
        printf("[Created] Writer#%d\n", id+1);
        acquire_write();
        printf("[Start]   Writer#%d 쓰기 시작\n", id+1);
        usleep(ms * 1000);
        printf("[Finish]  Writer#%d 쓰기 종료\n", id+1);
        release_write();
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("사용법: %s <sequence 파일>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("파일 열기 실패");
        return 1;
    }

    char line[32];
    while (fgets(line, sizeof(line), fp) && thread_count < MAX_THREADS) {
        types[thread_count] = line[0];
        times_ms[thread_count] = atoi(line + 1);
        thread_count++;
    }
    fclose(fp);

    init_lock();

    pthread_t ths[MAX_THREADS];
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&ths[i], NULL, thread_func, (void*)(long)i);
        usleep(100000);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(ths[i], NULL);
    }
    return 0;
}
