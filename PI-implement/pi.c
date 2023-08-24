#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_mutex_t lock1, lock2;

void *task1(void *arg)
{
    pthread_mutex_lock(&lock1);
    sleep(1);
    int t;
    struct sched_param param;
    pthread_t id = *(pthread_t *) arg;
    pthread_getschedparam(id, &t, &param);

    printf("task1 priority: %d\n", param.sched_priority);
    pthread_mutex_unlock(&lock1);
    return NULL;
}

void *task2(void *arg)
{
    // sleep(1);
    int t;
    struct sched_param param;
    pthread_t id = *(pthread_t *) arg;
    pthread_getschedparam(id, &t, &param);
    printf("task2 priority: %d\n", param.sched_priority);
    return NULL;
}

void *task3(void *arg)
{
    pthread_mutex_lock(&lock1);
    // sleep(1);
    int t;
    struct sched_param param;
    pthread_t id = *(pthread_t *) arg;
    pthread_getschedparam(id, &t, &param);
    printf("task3 priority: %d\n", param.sched_priority);
    pthread_mutex_unlock(&lock1);
    return NULL;
}

#define THREAD_COUNT 3

int main()
{
    pthread_t ids[THREAD_COUNT];
    pthread_attr_t attr[THREAD_COUNT];
    struct sched_param param[THREAD_COUNT];
    void *result;

    int t1=0, status;
    printf("priority(min-max): %d-%d\n", sched_get_priority_min(t1), sched_get_priority_max(t1));

    for (int i=0; i<THREAD_COUNT; i++) {
        if (pthread_attr_init(&attr[i])) {
            printf("Error when initializing attr %d\n", i);
            exit(1);
        }
    }

    for (int i=0; i<THREAD_COUNT; i++) {
        pthread_attr_getschedparam(&attr[i], &param[i]);
        pthread_attr_setschedpolicy(&attr[i], SCHED_RR);
        pthread_attr_setinheritsched(&attr[i], PTHREAD_EXPLICIT_SCHED);
        param[i].sched_priority = (i+1) * 15 + 1;
        pthread_attr_setschedparam(&attr[i], &param[i]);
    }

    pthread_mutex_init(&lock1, NULL);
    pthread_mutex_init(&lock2, NULL);

    pthread_create(&ids[0], &attr[0], task1, (void *) &ids[0]);
    pthread_create(&ids[2], &attr[2], task3, (void *) &ids[2]);
    pthread_create(&ids[1], &attr[1], task2, (void *) &ids[1]);


    for (int i=0; i<THREAD_COUNT; i++)
        pthread_join(ids[i], &result);
    
    for (int i=0; i<THREAD_COUNT; i++)
        pthread_attr_destroy(&attr[i]);

    return 0;
}