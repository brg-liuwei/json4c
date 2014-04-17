#include "jc_alloc.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#elif defined HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <unistd.h>
#include <assert.h>

#define JC_POOLMINSIZE 1024

struct jc_pool_data_s {
    char        *last;      /* last position of alloced data */
    char        *end;       /* end of current pool */
    short        fail;
    jc_pool_t   *next;
};

struct jc_pool_large_s {
    size_t             size;
    jc_pool_large_t   *next;
    void              *ptr;
};

struct jc_pool_s {
    jc_pool_data_t      data;
    size_t              max;     /* max data can alloc from pool */
    jc_pool_t          *current;
    jc_pool_large_t    *large;
    jc_pool_cln_t       cln;
};

static int jc_initialized = 0;
static size_t jc_pagesize;

#ifdef HAVE_MALLOC_H
#define jc_memalign memalign
#elif defined HAVE_STDLIB_H
static void *jc_memalign(size_t boundary, size_t size)
{
    int   rc;
    void *ptr;

    rc = posix_memalign(&ptr, JC_MEMALIGN, size);
    if (rc != 0 || ptr == NULL) {
        return NULL;
    }
    return ptr;
}
#endif

static void jc_init()
{
    if (jc_initialized == 0) {
        jc_pagesize = (size_t)getpagesize();
        jc_initialized = 1;
    }
}

jc_pool_t *jc_pool_create(size_t size)
{
    jc_pool_t    *p;

    jc_init();

    if (size < JC_POOLMINSIZE) {
        size = JC_POOLMINSIZE;
    }
    p = (jc_pool_t *)jc_memalign(JC_MEMALIGN, size);
    if (p == NULL) {
        return NULL;
    }
    
    p->data.last = (char *)jc_align((char *)p + sizeof(jc_pool_t));
    p->data.end = (char *)p + size;
    p->data.fail = 0;
    p->data.next = NULL;

    p->max = size - jc_align(sizeof(jc_pool_t)) > jc_pagesize ? 
        jc_pagesize : size - jc_align(sizeof(jc_pool_t));

    p->current = p;
    p->large = NULL;
    p->cln = NULL;

    return p;
}

void jc_pool_destroy(jc_pool_t *p)
{
    jc_pool_large_t  *large;
    jc_pool_t        *cur;

    assert(p != NULL);

    if (p->cln) {
        p->cln(p);
    }
    for (large = p->large; large != NULL; large = large->next) {
        if (large->ptr) {
            free(large->ptr);
        }
    }
    for (cur = p; cur != NULL; /* void */ ) {
        p = cur->data.next;
        free(cur);
        cur = p;
    }
}

static void *jc_pool_alloc_large(jc_pool_t *p, size_t size)
{
    jc_pool_large_t *large;

    assert(size > p->max);

    if ((large = jc_pool_alloc(p, sizeof(jc_pool_large_t))) == NULL) {
        return NULL;
    }
    large->next = p->large;
    p->large = large;
    large->size = size;
    return large->ptr = (jc_pool_large_t *)calloc(1, size);
}

static void *jc_pool_alloc_block(jc_pool_t *pool, size_t size)
{
    void        *m;
    size_t       pool_size;
    jc_pool_t   *p;

    assert(pool != NULL);

    pool_size = (size_t)(pool->data.end - (char *)pool);

    p = (jc_pool_t *)jc_memalign(JC_MEMALIGN, pool_size);

    if (p == NULL) {
        return NULL;
    }
    
    p->data.last = (char *)jc_align((char *)p + sizeof(jc_pool_data_t));
    p->data.end = (char *)p + pool_size;
    p->data.fail = 0;
    p->data.next = NULL;

    assert(p->data.end - p->data.last >= size);

    pool = pool->current;
    while (pool->data.next != NULL) {
        pool = pool->data.next;
    }
    pool->data.next = p;

    m = p->data.last;
    p->data.last += size;
    return m;
}

void *jc_pool_alloc(jc_pool_t *pool, size_t size)
{
    void       *m;
    jc_pool_t  *p;

    assert(pool != NULL);

    size = jc_align(size);

    if (size > pool->max) {
        return jc_pool_alloc_large(pool, size);
    }

    p = pool->current;

    do {
        if (p->data.end - p->data.last > size) {
            m = p->data.last;
            p->data.last += size;
            return m;
        }
        if (p->data.fail++ > 5 && p->data.next != NULL) {
            pool->current = p->data.next;
        }
        p = p->data.next;
    } while (p != NULL);

    return jc_pool_alloc_block(pool, size);
}

