#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define verify(x)                                                      \
    do {                                                               \
        int e;                                                         \
        if ((e = x) != 0) {                                            \
            fprintf(stderr, "%s(%d) %s: %s\n", __FILE__, __LINE__, #x, \
                    strerror(e));                                      \
            exit(1);                                                   \
        }                                                              \
    } while (0)

typedef int cmp_t(const void *, const void *);
static inline char *med3(char *, char *, char *, cmp_t *, void *);
static inline void swapfunc(char *, char *, int, int);

#define min(a, b)           \
    ({                      \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a < _b ? _a : _b;  \
    })

/* Qsort routine from Bentley & McIlroy's "Engineering a Sort Function" */
#define swapcode(TYPE, parmi, parmj, n) \
    {                                   \
        long i = (n) / sizeof(TYPE);    \
        TYPE *pi = (TYPE *) (parmi);    \
        TYPE *pj = (TYPE *) (parmj);    \
        do {                            \
            TYPE t = *pi;               \
            *pi++ = *pj;                \
            *pj++ = t;                  \
        } while (--i > 0);              \
    }

// static inline void swapfunc(char *a, char *b, int n, int swaptype)
// {
//     if (swaptype <= 1)
//         swapcode(long, a, b, n) else swapcode(char, a, b, n)
// }

#define swap(a, b)                         \
    do {                                   \
        if (swaptype == 0) {               \
            long t = *(long *) (a);        \
            *(long *) (a) = *(long *) (b); \
            *(long *) (b) = t;             \
        } else                             \
            swapfunc(a, b, es, swaptype);  \
    } while (0)

#define vecswap(a, b, n)                 \
    do {                                 \
        if ((n) > 0)                     \
            swapfunc(a, b, n, swaptype); \
    } while (0)

#define CMP(t, x, y) (cmp((x), (y)))

// static inline char *med3(char *a, char *b, char *c, cmp_t *cmp, void *thunk)
// {
//     return CMP(thunk, a, b) < 0
//                ? (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a))
//                : (CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c));
// }

/* We use some elaborate condition variables and signalling
 * to ensure a bound of the number of active threads at
 * 2 * maxthreads and the size of the thread data structure
 * to maxthreads.
 */

/* Condition of starting a new thread. */
enum thread_state {
    ts_idle, /* Idle, waiting for instructions. */
    ts_work, /* Has work to do. */
    ts_term  /* Asked to terminate. */
};

/* Variant part passed to qsort invocations. */
struct qsort {
    enum thread_state st;   /* For coordinating work. */
    struct common *common;  /* Common shared elements. */
    void *a;                /* Array base. */
    size_t n;               /* Number of elements. */
    pthread_t id;           /* Thread id. */
    pthread_mutex_t mtx_st; /* For signalling state change. */
    pthread_cond_t cond_st; /* For signalling state change. */
};

/* Invariant common part, shared across invocations. */
struct common {
    int swaptype;           /* Code to use for swapping */
    size_t es;              /* Element size. */
    void *thunk;            /* Thunk for qsort_r */
    cmp_t *cmp;             /* Comparison function */
    int nthreads;           /* Total number of pool threads. */
    int idlethreads;        /* Number of idle threads in pool. */
    int forkelem;           /* Minimum number of elements for a new thread. */
    struct qsort *pool;     /* Fixed pool of threads. */
    pthread_mutex_t mtx_al; /* For allocating threads in the pool. */
};

static void *qsort_thread(void *p);
void qsort_mt(void *a,
              size_t n,
              size_t es,
              cmp_t *cmp,
              int maxthreads,
              int forkelem);

#define thunk NULL
static struct qsort *allocate_thread(struct common *c);
static void qsort_algo(struct qsort *qs);