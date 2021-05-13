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
    pthread_mutex_init(&qptr->job_mutex, NULL);
    pthread_cond_init(&qptr->job_cv, NULL);
}
static ssize_t queue_push(struct jobqueue* qptr, struct job* job) {
    if (qptr->len >= N_THREADS_MAX) {
        LOG_ERR("pushing a full queue, threads alive %u\n",
                threadpool.n_threads_free);
        // notify threads to fetch a job
        pthread_cond_signal(&threadpool.queue.job_cv);
        return -1;
    }
    QINDEX_INC(qptr->back);
    qptr->queue[qptr->back] = job;
    qptr->len++;
    LOG_INFO("threads alive: %u\n", threadpool.n_threads_free);
    pthread_cond_signal(&threadpool.queue.job_cv);
    return 1;
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
static struct job* pull_job(struct jobqueue* qptr) {
    struct job* jptr = NULL;
    pthread_mutex_lock(&qptr->job_mutex);
    while (!qptr->len) {
        pthread_cond_wait(&qptr->job_cv, &qptr->job_mutex);
    }

    if (qptr->len) {
        // has job avaliable, get it
        jptr = queue_front(qptr);
        queue_pop(qptr);
    }
    pthread_mutex_unlock(&qptr->job_mutex);
    return jptr;
}

static void thread_work() {
    struct job* job = NULL;
    for (;;) {
        // continuously fetch new jobs from the queue
        job = pull_job(&threadpool.queue);
        // decrease the avaliable thread count
        pthread_mutex_lock(&threadpool.cont_mutex);
        threadpool.n_threads_free--;
        pthread_mutex_unlock(&threadpool.cont_mutex);
        if (job) {
            LOG_INFO("thread %lu is doing the job\n", pthread_self());
            // get the job done
            job->fun(job->arg);
            job = NULL;
        }
        pthread_mutex_lock(&threadpool.cont_mutex);
        threadpool.n_threads_free++;
        if (threadpool.n_threads_free == threadpool.n_threads) {
            // keep at least one thread is asking for job
            pthread_cond_signal(&threadpool.queue.job_cv);
        }

        pthread_mutex_unlock(&threadpool.cont_mutex);
    }
}

ssize_t init_threadpool() {
    LOG_INFO("initializing threadpool...\n");
    memset(&threadpool, 0, sizeof(threadpool));
    queue_init(&threadpool.queue);
    // initing threads
    threadpool.n_threads = DEFAULT_N_THREADS;
    pthread_mutex_init(&threadpool.cont_mutex, NULL);
    for (size_t i = 0; i < threadpool.n_threads; i++) {
        if (pthread_create(&threadpool.threads[i], NULL, thread_work, NULL) <
            0) {
            LOG_ERR("failed creating thread %d\n", i);
            return -1;
        } else {
            pthread_detach(threadpool.threads[i]);
        }
    }
    threadpool.n_threads_free = threadpool.n_threads;
    return 1;
}

ssize_t threadpool_add_job(struct job* job) {
    pthread_mutex_lock(&threadpool.queue.job_mutex);
    ssize_t ret = queue_push(&threadpool.queue, job);
    pthread_mutex_unlock(&threadpool.queue.job_mutex);
    return ret;
}
