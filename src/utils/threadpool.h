#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#define N_THREADS_MAX 128
#define DEFAULT_N_THREADS 12

struct job {
    void (*fun)(void* arg);
    void* arg;
};

struct jobqueue {
    struct job* queue[N_THREADS_MAX];
    uint8_t len;
    pthread_mutex_t job_mutex;
    pthread_cond_t job_cv;
    uint8_t front, back;
};

struct threadpool {
    pthread_t threads[N_THREADS_MAX];
    uint8_t n_threads_free;
    uint8_t n_threads;
    // condition variable to wake up threads for work
    pthread_mutex_t cont_mutex;
    struct jobqueue queue;
};

extern struct threadpool threadpool;
struct job* create_job(void (*fun)(void* arg), void*);

ssize_t init_threadpool();

// return -1 if the queue is filled up
ssize_t threadpool_add_job(struct job*);

#endif