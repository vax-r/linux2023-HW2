// this file is use for futex quick sort

#include "qsort_ft.h"

static inline char *med3(char *, char *, char *, cmp_t *, void *);
static inline void swapfunc(char *, char *, int, int);

static void *qsort_thread(void *p);
static struct qsort *allocate_thread(struct common *c);
static void qsort_algo(struct qsort *qs);

static inline void swapfunc(char *a, char *b, int n, int swaptype)
{
    if (swaptype <= 1)
        swapcode(long, a, b, n) else swapcode(char, a, b, n)
}

static inline char *med3(char *a, char *b, char *c, cmp_t *cmp, void *tnk)
{
    return CMP(tnk, a, b) < 0
               ? (CMP(tnk, b, c) < 0 ? b : (CMP(tnk, a, c) < 0 ? c : a))
               : (CMP(tnk, b, c) > 0 ? b : (CMP(tnk, a, c) < 0 ? a : c));
}

/* The multithreaded qsort public interface */
void qsort_mt(void *a,
              size_t n,
              size_t es,
              cmp_t *cmp,
              int maxthreads,
              int forkelem)
{
    struct qsort *qs;
    struct common c;
    int i, islot;
    bool bailout = true;

    if (n < forkelem)
        goto f1;
    errno = 0;
    /* Try to initialize the resources we need. */
    mutex_init(&c.fmtx_al);
    if ((c.pool = calloc(maxthreads, sizeof(struct qsort))) == NULL)
        goto f2;
    for (islot = 0; islot < maxthreads; islot++) {
        qs = &c.pool[islot];
        mutex_init(&qs->fmtx_st);
        cond_init(&qs->fcond_st);
        qs->st = ts_idle;
        qs->common = &c;
        if (pthread_create(&qs->id, NULL, qsort_thread, qs) != 0) {
            // verify(pthread_mutex_destroy(&qs->fmtx_st));
            // verify(pthread_cond_destroy(&qs->fcond_st));
            goto f3;
        }
    }

    /* All systems go. */
    bailout = false;

    /* Initialize common elements. */
    c.swaptype = ((char *) a - (char *) 0) % sizeof(long) || es % sizeof(long)
                     ? 2
                 : es == sizeof(long) ? 0
                                      : 1;
    c.es = es;
    c.cmp = cmp;
    c.forkelem = forkelem;
    c.idlethreads = c.nthreads = maxthreads;

    /* Hand out the first work batch. */
    qs = &c.pool[0];
    mutex_lock(&qs->fmtx_st);
    qs->a = a;
    qs->n = n;
    qs->st = ts_work;
    c.idlethreads--;
    cond_signal(&qs->fcond_st, &qs->fmtx_st);
    mutex_unlock(&qs->fmtx_st);

    /* Wait for all threads to finish, and free acquired resources. */
f3:
    for (i = 0; i < islot; i++) {
        qs = &c.pool[i];
        if (bailout) {
            mutex_lock(&qs->fmtx_st);
            qs->st = ts_term;
            cond_signal(&qs->fcond_st, &qs->fmtx_st);
            mutex_unlock(&qs->fmtx_st);
        }
        verify(pthread_join(qs->id, NULL));
        // verify(pthread_mutex_destroy(&qs->fmtx_st));
        // verify(pthread_cond_destroy(&qs->fcond_st));
    }
    free(c.pool);
f2:
    // verify(pthread_mutex_destroy(&c.fmtx_al));
    if (bailout) {
        fprintf(stderr, "Resource initialization failed; bailing out.\n");
    f1:
        qsort(a, n, es, cmp);
    }
}

/* Allocate an idle thread from the pool, lock its mutex, change its state to
 * work, decrease the number of idle threads, and return a pointer to its data
 * area.
 * Return NULL, if no thread is available.
 */
static struct qsort *allocate_thread(struct common *c)
{
    mutex_lock(&c->fmtx_al);
    for (int i = 0; i < c->nthreads; i++) {
        mutex_lock(&c->pool[i].fmtx_st);
        if (c->pool[i].st == ts_idle) {
            c->idlethreads--;
            c->pool[i].st = ts_work;
            mutex_unlock(&c->pool[i].fmtx_st);
            mutex_unlock(&c->fmtx_al);
            return (&c->pool[i]);
        }
        mutex_unlock(&c->pool[i].fmtx_st);
    }
    mutex_unlock(&c->fmtx_al);
    return (NULL);
}

/* Thread-callable quicksort. */
static void qsort_algo(struct qsort *qs)
{
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int d, r, swaptype, swap_cnt;
    void *a;      /* Array of elements. */
    size_t n, es; /* Number of elements; size. */
    cmp_t *cmp;
    int nl, nr;
    struct common *c;
    struct qsort *qs2;

    /* Initialize qsort arguments. */
    mutex_lock(&qs->fmtx_st);
    c = qs->common;
    mutex_lock(&c->fmtx_al);
    es = c->es;
    cmp = c->cmp;
    swaptype = c->swaptype;
    mutex_unlock(&c->fmtx_al);
    a = qs->a;
    n = qs->n;
    mutex_unlock(&qs->fmtx_st);
top:
    /* From here on qsort(3) business as usual. */
    swap_cnt = 0;
    if (n < 7) {
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es)
                swap(pl, pl - es);
        return;
    }
    pm = (char *) a + (n / 2) * es;
    if (n > 7) {
        pl = (char *) a;
        pn = (char *) a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
            pm = med3(pm - d, pm, pm + d, cmp, thunk);
            pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
        }
        pm = med3(pl, pm, pn, cmp, thunk);
    }
    swap(a, pm);
    pa = pb = (char *) a + es;

    pc = pd = (char *) a + (n - 1) * es;
    for (;;) {
        while (pb <= pc && (r = CMP(thunk, pb, a)) <= 0) {
            if (r == 0) {
                swap_cnt = 1;
                swap(pa, pb);
                pa += es;
            }
            pb += es;
        }
        while (pb <= pc && (r = CMP(thunk, pc, a)) >= 0) {
            if (r == 0) {
                swap_cnt = 1;
                swap(pc, pd);
                pd -= es;
            }
            pc -= es;
        }
        if (pb > pc)
            break;
        swap(pb, pc);
        swap_cnt = 1;
        pb += es;
        pc -= es;
    }

    pn = (char *) a + n * es;
    r = min(pa - (char *) a, pb - pa);
    vecswap(a, pb - r, r);
    r = min(pd - pc, pn - pd - es);
    vecswap(pb, pn - r, r);

    if (swap_cnt == 0) { /* Switch to insertion sort */
        r = 1 + n / 4;   /* n >= 7, so r >= 2 */
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es) {
                swap(pl, pl - es);
                if (++swap_cnt > r)
                    goto nevermind;
            }
        return;
    }

nevermind:
    nl = (pb - pa) / es;
    nr = (pd - pc) / es;

    /* Now try to launch subthreads. */
    if (nl > c->forkelem && nr > c->forkelem &&
        (qs2 = allocate_thread(c)) != NULL) {
        mutex_lock(&qs->fmtx_st);
        qs2->a = a;
        qs2->n = nl;
        cond_signal(&qs->fcond_st, &qs->fmtx_st);
        mutex_unlock(&qs->fmtx_st);
    } else if (nl > 0) {
        qs->a = a;
        qs->n = nl;
        qsort_algo(qs);
    }
    if (nr > 0) {
        a = pn - nr * es;
        n = nr;
        goto top;
    }
}


/* Thread-callable quicksort. */
static void *qsort_thread(void *p)
{
    struct qsort *qs, *qs2;
    int i;
    struct common *c;

    qs = p;
    c = qs->common;
again:
    /* Wait for work to be allocated. */
    mutex_lock(&qs->fmtx_st);
    while (qs->st == ts_idle)
        cond_wait(&qs->fcond_st, &qs->fmtx_st);
    // verify(HHHH);
    mutex_unlock(&qs->fmtx_st);
    if (qs->st == ts_term) {
        return NULL;
    }
    assert(qs->st == ts_work);

    qsort_algo(qs);

    mutex_lock(&c->fmtx_al);
    qs->st = ts_idle;
    c->idlethreads++;
    if (c->idlethreads == c->nthreads) {
        for (i = 0; i < c->nthreads; i++) {
            qs2 = &c->pool[i];
            if (qs2 == qs)
                continue;
            mutex_lock(&qs2->fmtx_st);
            qs2->st = ts_term;
            cond_signal(&qs2->fcond_st, &qs2->fmtx_st);
            mutex_unlock(&qs2->fmtx_st);
        }
        mutex_unlock(&c->fmtx_al);
        return NULL;
    }
    mutex_unlock(&c->fmtx_al);
    goto again;
}