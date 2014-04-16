#ifndef __JC_ALLOC_H__
#define __JC_ALLOC_H__

/* 
 * Thanks for Igor Sysoev. 
 * I followed your memory pool of nginx 
 * */

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#else
typedef unsigned long size_t;
#endif

#define JC_MEMALIGN sizeof(unsigned long)
#define jc_align(x) \
    (((size_t)(x) + (JC_MEMALIGN - 1)) & ~(JC_MEMALIGN - 1))

typedef struct jc_pool_data_s  jc_pool_data_t;
typedef struct jc_pool_large_s jc_pool_large_t;
typedef struct jc_pool_s       jc_pool_t;

typedef void (*jc_pool_cln_t)(void *);

jc_pool_t *jc_pool_create(size_t  size);
void jc_pool_destroy(jc_pool_t *pool);
void *jc_pool_alloc(jc_pool_t *pool, size_t size);

#endif

