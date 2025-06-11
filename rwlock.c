#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

#define MAX_THREADS 100

typedef struct _rwlock_t {
    sem_t mutex;     // protects readers count
    sem_t queue;     // fairness queue: readers/writers 순서 보장
    sem_t rw_mutex;  // actual read/write 배타 제어
    int readers;     // 현재 읽기 중인 reader 수
} rwlock_t;

void rwlock_init(rwlock_t *rw);
void rwlock_acquire_readlock(rwlock_t *rw);
void rwlock_release_readlock(rwlock_t *rw);
void rwlock_acquire_writelock(rwlock_t *rw);
void rwlock_release_writelock(rwlock_t *rw);

static rwlock_t rwlock;
static char types[MAX_THREADS];
static int  times_ms[MAX_THREADS];
static int  thread_count = 0;

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->mutex,    0, 1);
    sem_init(&rw->queue,    0, 1);
    sem_init(&rw->rw_mutex, 0, 1);
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    sem_wait(&rw->queue);             // 순서 대기실 입장
    sem_wait(&rw->mutex);             // readers 수 보호
    rw->readers++;
    if (rw->readers == 1) {
        sem_wait(&rw->rw_mutex);      // 첫 번째 reader가 writer 잠금 획득
    }
    sem_post(&rw->mutex);
    sem_post(&rw->queue);             // 다음 순서자 허용
}

void rwlock_release_readlock(rwlock_t *rw) {
    sem_wait(&rw->mutex);
    rw->readers--;
    if (rw->readers == 0) {
        sem_post(&rw->rw_mutex);      // 마지막 reader가 writer 잠금 해제
    }
    sem_post(&rw->mutex);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    sem_wait(&rw->queue);             // 순서 대기실 입장
    sem_wait(&rw->rw_mutex);          // writer 전용 잠금 획득
    sem_post(&rw->queue);             // 다음 순서자 허용
}

void rwlock_release_writelock(rwlock_t *rw) {
    sem_post(&rw->rw_mutex);
}

void* thread_func(void* arg) {
    int id   = (int)(long)arg;
    char t   = types[id];
    int  ms  = times_ms[id];

    if (t == 'R') {
        printf("[Created] Reader#%d\n", id+1);
        rwlock_acquire_readlock(&rwlock);
        printf("[Start]   Reader#%d 읽기 시작\n", id+1);
        usleep(ms * 1000);
        printf("[Finish]  Reader#%d 읽기 종료\n", id+1);
        rwlock_release_readlock(&rwlock);

    } else {  // 'W'
        printf("[Created] Writer#%d\n", id+1);
        rwlock_acquire_writelock(&rwlock);
        printf("[Start]   Writer#%d 쓰기 시작\n", id+1);
        usleep(ms * 1000);
        printf("[Finish]  Writer#%d 쓰기 종료\n", id+1);
        rwlock_release_writelock(&rwlock);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "사용법: %s <sequence 파일>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("파일 열기 실패");
        return EXIT_FAILURE;
    }

    char line[32];
    while (fgets(line, sizeof(line), fp) && thread_count < MAX_THREADS) {
        types[thread_count]    = line[0];
        times_ms[thread_count] = atoi(line + 1);
        thread_count++;
    }
    fclose(fp);

    // rwlock 초기화
    rwlock_init(&rwlock);

    // 스레드 생성
    pthread_t ths[MAX_THREADS];
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&ths[i], NULL, thread_func, (void*)(long)i);
        usleep(100000);  // 100ms 간격
    }

    // 스레드 종료 대기
    for (int i = 0; i < thread_count; i++) {
        pthread_join(ths[i], NULL);
    }

    return EXIT_SUCCESS;
}
