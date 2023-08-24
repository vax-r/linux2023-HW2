#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define SCHED_POLICY SCHED_OTHER
#define INHERIT_SCHED PTHREAD_EXPLICIT_SCHED

pthread_mutex_t lock1, lock2;

typedef struct {
    pthread_t tid;
    pthread_attr_t tattr;
    struct sched_param param;

} pi_thread;

void pi_thread_init (pi_thread *t, int priority)
{
    pthread_attr_init(&t->tattr);
    pthread_attr_setschedpolicy(&t->tattr, SCHED_POLICY);
    t->param.sched_priority = priority;
    pthread_attr_setschedparam(&t->tattr, &t->param);
    pthread_attr_setinheritsched(&t->tattr, INHERIT_SCHED);
}

void *normal_task1 (void *arg)
{
    pthread_mutex_lock(&lock1);
    // sleep(1);
    pi_thread *t = (pi_thread *) arg;
    int policy;
    struct sched_param param;
    int rc = pthread_getschedparam(t->tid, &policy, &param);
    if (rc) {
        printf("Error getting param\n");
        exit(1);
    }
    printf("priority is %d\n", param.sched_priority);
    pthread_mutex_unlock(&lock1);
    return NULL;
}

void *normal_task2 (void *arg)
{
    pthread_mutex_lock(&lock2);
    sleep(1);
    pi_thread *t = (pi_thread *) arg;
    printf("priority is %d\n", t->param.sched_priority);
    pthread_mutex_unlock(&lock2);
    return NULL;
}

void *normal_task3 (void *arg)
{
    pthread_mutex_lock(&lock1);
    sleep(1);
    pi_thread *t = (pi_thread *) arg;
    printf("priority is %d\n", t->param.sched_priority);
    pthread_mutex_unlock(&lock1);
    return NULL;
}

#define THREAD_NUMBER 3

int main (int argc, char *argv[])
{
    pi_thread ids[THREAD_NUMBER];
    pthread_attr_t attr;
    void *result;

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, INHERIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_POLICY);
    

    for (int i = 0; i < THREAD_NUMBER; ++i)
        pi_thread_init(&ids[i], (THREAD_NUMBER - i) * 10);
    
    // pthread_mutexattr_t mtx_attr;
    // pthread_mutexattr_setprotocol(&mtx_attr, PTHREAD_PRIO_INHERIT);
    // pthread_mutex_init(&lock1, &mtx_attr);

    pthread_create(&ids[2].tid, &ids[2].tattr , normal_task3, (void *) &ids[2]);

    pthread_create(&ids[1].tid, &ids[1].tattr, normal_task2, (void *) &ids[1]);

    pthread_create(&ids[0].tid, &ids[0].tattr, normal_task1, (void *) &ids[0]);

    for(int i=0; i<THREAD_NUMBER; i++)
        pthread_join(ids[i].tid, &result);
    return 0;
}