#pragma once

// #if USE_PTHREADS

// #include <pthread.h>

// #define mutex_t pthread_mutex_t
// #define mutex_init(m) pthread_mutex_init(m, NULL)
// #define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
// #define mutex_trylock(m) (!pthread_mutex_trylock(m))
// #define mutex_lock pthread_mutex_lock
// #define mutex_unlock pthread_mutex_unlock

// #else

#include <stdbool.h>
#include "atomic.h"
#include "spinlock.h"

#include <stdio.h>

// futex.h

#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline void futex_wait(atomic int *futex, int value)
{
    syscall(SYS_futex, futex, FUTEX_WAIT_PRIVATE, value, NULL);
}

/* Wake up 'limit' threads currently waiting on 'futex' */
static inline void futex_wake(atomic int *futex, int limit)
{
    syscall(SYS_futex, futex, FUTEX_WAKE_PRIVATE, limit);
}

/* Wake up 'limit' waiters, and re-queue the rest onto a different futex */
static inline void futex_requeue(atomic int *futex,
                                 int limit,
                                 atomic int *other)
{
    syscall(SYS_futex, futex, FUTEX_REQUEUE_PRIVATE, limit, INT_MAX, other);
}
// end here

typedef struct {
    atomic int state;
    pthread_t *owner;
    spinlock_t wait_lock;
} mutex_t;

enum {
    MUTEX_LOCKED = 1 << 0,
    MUTEX_SLEEPING = 1 << 1,
};

#define MUTEX_INITIALIZER \
    {                     \
        .state = 0        \
    }

static inline void mutex_init(mutex_t *mutex)
{
    atomic_init(&mutex->state, 0);
    mutex->owner = NULL;
    spin_init(&mutex->wait_lock);
}

static bool mutex_trylock(mutex_t *mutex)
{
    int state = load(&mutex->state, relaxed);
    if (state & MUTEX_LOCKED)
        return false;

    state = fetch_or(&mutex->state, MUTEX_LOCKED, relaxed);
    if (state & MUTEX_LOCKED)
        return false;

    thread_fence(&mutex->state, acquire);
    return true;
}

static inline void mutex_lock(mutex_t *mutex, pthread_t *caller)
{
#define MUTEX_SPINS 128
    for (int i = 0; i < MUTEX_SPINS; ++i) {
        if (mutex_trylock(mutex)) {
            spin_lock(&mutex->wait_lock);
            mutex->owner = caller;
            spin_unlock(&mutex->wait_lock);
            return;
        }
        spin_hint();
    }

    int state = exchange(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING, relaxed);

    while (state & MUTEX_LOCKED) {

        spin_lock(&mutex->wait_lock);
        printf("The owner of the lock now is %lu\n", *mutex->owner);
        int t_c, t_o;
        struct sched_param param_c, param_o;
        pthread_getschedparam(*mutex->owner, &t_o, &param_o);
        pthread_getschedparam(*caller, &t_c, &param_c);
        if(param_o.sched_priority < param_c.sched_priority) {
            param_o.sched_priority = param_c.sched_priority;
            pthread_setschedparam(*mutex->owner, t_o, &param_o);
        }

        spin_unlock(&mutex->wait_lock);

        futex_wait(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING);
        state = exchange(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING, relaxed);
    }
    spin_lock(&mutex->wait_lock);
    mutex->owner = caller;
    spin_unlock(&mutex->wait_lock);
    thread_fence(&mutex->state, acquire);
}

static inline void mutex_unlock(mutex_t *mutex)
{
    int state = exchange(&mutex->state, 0, release);
    spin_lock(&mutex->wait_lock);
    mutex->owner = NULL;
    spin_unlock(&mutex->wait_lock);
    if (state & MUTEX_SLEEPING)
        futex_wake(&mutex->state, 1);
}

// #endif