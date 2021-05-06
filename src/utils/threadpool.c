#include "utils/threadpool.h"

#include <stdlib.h>

#include "utils/logging.h"
#define QINDEX_INC(x) (x = (x + 1) % N_THREADS_MAX)
struct threadpool threadpool;
static void queue_init(struct jobqueue* qptr) {
    memset(qptr, 0, sizeof(struct jobqueue));
    qptr->front = 0;
    qptr->back = N_THREADS_MAX - 1;
    qptr->len = 0;
    pthread_mutex_init(&qptr->joblock, NULL);
}
static ssize_t queue_push(struct jobqueue* qptr, struct job* job) {
    if (qptr->len >= N_THREADS_MAX) {
        LOG_ERR("pushing a full queue\n");
        return -1;
    }
    QINDEX_INC(qptr->back);
    qptr->queue[qptr->back] = job;
    qptr->len++;
}
static struct job* queue_front(struct jobqueue* qptr) {
    if (qptr->len == 0) {
        LOG_ERR("requesting an empty queue\n");
        return NULL;
    }
    return qptr->queue[qptr->front];
}
static void queue_pop(struct jobqueue* qptr) {
    if (qptr->len == 0) {
        LOG_ERR("poping an empty queue\n");
        return;
    }
    QINDEX_INC(qptr->front);
    qptr->len--;
}
struct job* create_job(void (*fun)(void* arg), void* arg) {
    struct job* job = malloc(sizeof(struct job));
    job->arg = arg;
    job->fun = fun;
    return job;
}

static void thread_work() {
    struct job* job = NULL;
    for (;;) {
        // continuously fetch new jobs from the queue
        pthread_mutex_lock(&threadpool.queue.joblock);
        if (threadpool.queue.len) {
            // has job avaliable, get it
            job = queue_front(&threadpool.queue);
            queue_pop(&threadpool.queue);
        }
        pthread_mutex_unlock(&threadpool.queue.joblock);
        if (job) {
            // get the job done
            job->fun(job->arg);
            job = NULL;
        }
    }
}

ssize_t init_threadpool() {
    LOG_INFO("initializing threadpool...\n");
    memset(&threadpool, 0, sizeof(threadpool));
    queue_init(&threadpool.queue);
    // initing threads
    threadpool.n_threads = DEFAULT_N_THREADS;
    for (size_t i = 0; i < threadpool.n_threads; i++) {
        if (pthread_create(&threadpool.threads[i], NULL, thread_work, NULL) <
            0) {
            threadpool.threads_alive[i] = 1;
            LOG_ERR("failed creating thread %d\n", i);
            return -1;
        } else {
            pthread_detach(threadpool.threads[i]);
        }
    }
}

ssize_t threadpool_add_job(struct job* job) {
    pthread_mutex_lock(&threadpool.queue.joblock);
    ssize_t ret = queue_push(&threadpool.queue, job);
    pthread_mutex_unlock(&threadpool.queue.joblock);
    return ret;
}
