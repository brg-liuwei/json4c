#include "jc_type.h"
#include "jc_alloc.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define JC_MEMSIZE 1024
#define JC_INCSTEP 16

static int __jc_json_str(jc_json_t *js, char *p);
static size_t __jc_json_val_size(jc_val_t *val);
static size_t jc_json_size(jc_json_t *js);

jc_json_t *jc_json_create()
{
    jc_pool_t   *pool;
    jc_json_t   *json;

    if ((pool = jc_pool_create(JC_MEMSIZE)) == NULL) {
        return NULL;
    }
    if ((json = jc_pool_alloc(pool, sizeof(jc_json_t))) == NULL) {
        goto free;
    }
    json->size = 0;
    json->free = 0;
    json->keys = NULL;
    json->vals = NULL;
    json->pool = pool;
    return json;

free:
    jc_pool_destroy(pool);
    return NULL;
}

void jc_json_destroy(jc_json_t *js)
{
    size_t      i, j;
    jc_val_t   *val;

    for (i = 0; i != js->size; ++i) {
        val = js->vals[i];
        if (val->type == JC_JSON) {
            /* careful here, if more than 2 js is added in this json, program will crash */
            jc_json_destroy(val->data.j);
        }
        if (val->type == JC_ARRAY && val->data.a->size != 0) {
            if (val->data.a->value[0]->type == JC_JSON) {
                for (j = 0; j != val->data.a->size; ++j) {
                    jc_json_destroy(val->data.a->value[j]->data.j);
                }
            }
        }
    }
    jc_pool_destroy(js->pool);
}

static int jc_kv_incr(jc_json_t *js)
{
    jc_key_t   **new_keys;
    jc_val_t   **new_vals;
    jc_pool_t   *pool;

    assert(js != NULL);
    assert(js->free == 0);

    pool = js->pool;
    new_keys = jc_pool_alloc(pool, sizeof(jc_key_t *) * (js->size + JC_INCSTEP));
    new_vals = jc_pool_alloc(pool, sizeof(jc_val_t *) * (js->size + JC_INCSTEP));
    if (new_keys == NULL || new_vals == NULL) {
        return -1;
    }
    js->keys = new_keys;
    js->vals = new_vals;
    js->free += JC_INCSTEP;
    return 0;
}

static void jc_kv_insert(jc_json_t *js, jc_key_t *key, jc_val_t *val)
{
    size_t   i;

    for (i = js->size; i != 0; --i) {
        if (strcmp(key->body, js->keys[i-1]->body) < 0) {
            js->keys[i] = js->keys[i-1];
            js->vals[i] = js->vals[i-1];
        } else {
            break;
        }
    }
    js->keys[i] = key;
    js->vals[i] = val;
    ++js->size;
    --js->free;
}

//static int jc_bsearch_key(jc_json_t *js, const char *key)
//{
//    int      l, r, m, rc;
//
//    for (l = 0, r = js->size - 1; l <= r; /* void */ ) {
//        m = l + ((r -l) >> 1);
//        rc = strcmp(key, js->keys[m]->body);
//        if (rc > 0) {
//            l = m + 1;
//        } else if (rc < 0) {
//            r = m - 1;
//        } else {
//            return m;
//        }
//    }
//    return -1;
//}


static int jc_array_incr(jc_array_t *arr, jc_pool_t *pool)
{
    size_t      i;
    jc_val_t  **v;

    assert(arr->free == 0);

    v = (jc_val_t **)jc_pool_alloc(pool, (arr->size + JC_INCSTEP) * sizeof (jc_val_t *));
    if (v == NULL) {
        return -1;
    }

    for (i = 0; i != arr->size; ++i) {
        v[i] = arr->value[i];
    }

    arr->value = v;
    arr->free = JC_INCSTEP;

    return 0;
}

static int jc_array_append(jc_array_t *arr, jc_pool_t *pool, jc_val_t *val)
{
    if (arr->size != 0
            && arr->value[0]->type != val->type)
    {
        return -1;
    }

    if (arr->free == 0
            && jc_array_incr(arr, pool) == -1)
    {
        /* array incr err */
        return -1;
    }

    arr->value[arr->size] = val;
    ++arr->size;
    --arr->free;
    return 0;
}

static jc_array_t *jc_array_create(jc_pool_t *pool)
{
    jc_array_t  *arr;

    if ((arr = jc_pool_alloc(pool, sizeof(jc_array_t))) == NULL) {
        return NULL;
    }
    arr->size = 0;
    arr->free = 0;
    arr->value = NULL;
    return arr;
}

static int jc_trans_array(jc_json_t *js, int idx)
{
    jc_val_t   *val, *new_val;
    jc_array_t *arr;

    if ((arr = jc_array_create(js->pool)) == NULL) {
        return -1;
    }
    if ((new_val = jc_pool_alloc(js->pool, sizeof(jc_val_t))) == NULL) {
        return -1;
    }
    val = js->vals[idx];

    new_val->type = JC_ARRAY;
    new_val->data.a = arr;
    js->vals[idx] = new_val;

    return jc_array_append(arr, js->pool, val);
}

//static int __jc_add_bool(jc_json_t *js, const char *k, int bl)
//{
//    size_t       key_size, key_len;
//    jc_key_t    *key;
//    jc_val_t    *val;
//
//    if (js->free == 0) {
//        if (jc_kv_incr(js) == -1) {
//            return -1;
//        }
//    }
//
//    key_len = strlen(k) + 1;
//    key_size = jc_align(sizeof(jc_key_t) + key_len);
//    key = jc_pool_alloc(js->pool, key_size);
//    val = jc_pool_alloc(js->pool, sizeof(jc_val_t));
//    if (key == NULL || val == NULL) {
//        return -1;
//    }
//
//    /* key assign */
//    key->size = key_len;   /* including the terminating '\0' */
//    key->free = key_size - key_len;
//    strncpy(key->body, k, key->size + key->free);
//
//    /* value assign */
//    val->type = JC_BOOL;
//    val->data.b = (short)(bl != 0);
//
//    jc_kv_insert(js, key, val);
//    return 0;
//}

//static int jc_array_append_bool(jc_json_t *js, int idx, int bl)
//{
//    jc_val_t  *val;
//
//    assert(js->vals[idx]->type == JC_ARRAY);
//
//    if ((val = jc_pool_alloc(js->pool, sizeof(jc_val_t))) == NULL) {
//        return -1;
//    }
//    val->type = JC_BOOL;
//    val->data.b = (short)(bl != 0);
//    return jc_array_append(js->vals[idx]->data.a, js->pool, val);
//}

static int jc_bsearch_key(jc_json_t *js, jc_key_t *key)
{
    int      l, r, m, rc;

    for (l = 0, r = js->size - 1; l <= r; /* void */ ) {
        m = l + ((r -l) >> 1);
        rc = strcmp(key->body, js->keys[m]->body);
        if (rc > 0) {
            l = m + 1;
        } else if (rc < 0) {
            r = m - 1;
        } else {
            return m;
        }
    }
    return -1;
}

static int jc_json_add_kv(jc_json_t *js, jc_key_t *key, jc_val_t *val)
{
    int idx;

    idx = jc_bsearch_key(js, key);

    if (idx == -1) {
        if (js->free == 0
                && jc_kv_incr(js) != 0)
        {
            return -1;
        }
        jc_kv_insert(js, key, val);
        return 0;
    }

    /* key exist */
    if (js->vals[idx]->type == val->type) {
        if (jc_trans_array(js, idx) != 0) {
            return -1;
        }
        assert(js->vals[idx]->type == JC_ARRAY);
        return jc_array_append(js->vals[idx]->data.a, js->pool, val);
    }

    if (js->vals[idx]->type == JC_ARRAY) {
        if (js->vals[idx]->data.a->size == 0
                || js->vals[idx]->data.a->value[0]->type == val->type)
        {
            return jc_array_append(js->vals[idx]->data.a, js->pool, val);
        }
    }

    /* type error */
    return -1;
}

static jc_key_t *jc_key(jc_pool_t *pool, const char *key)
{
    size_t     key_len, key_size;
    jc_key_t  *k;

    key_len = strlen(key);
    key_size = jc_align(sizeof(jc_key_t) + key_len + 1);  /* add the teminating zero */
    k = jc_pool_alloc(pool, key_size);
    if (k != NULL) {
        k->size = key_len + 1;
        k->free = key_size - k->size - sizeof(jc_key_t);
        strncpy(k->body, key, k->size + k->free);
    }
    return k;
}

int jc_json_add_int(jc_json_t *js, const char *key, int64_t i)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_INT;
    v->data.i = i;

    return jc_json_add_kv(js, k, v);
}

int jc_json_add_bool(jc_json_t *js, const char *key, int bl)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_BOOL;
    v->data.b = (short)(bl != 0);

    return jc_json_add_kv(js, k, v);
}

int jc_json_add_null(jc_json_t *js, const char *key)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_NULL;

    return jc_json_add_kv(js, k, v);
}

int jc_json_add_str(jc_json_t *js, const char *key, const char *val)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);
    assert(val != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_STR;
    /* jc_key() returns jc_str */
    if ((v->data.s = jc_key(js->pool, val)) == NULL) {
        return -1;
    }

    return jc_json_add_kv(js, k, v);
}

int jc_json_add_array(jc_json_t *js, const char *key)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_ARRAY;
    if ((v->data.a = jc_array_create(js->pool)) == NULL) {
        return -1;
    }

    return jc_json_add_kv(js, k, v);
}

int jc_json_add_json(jc_json_t *js, const char *key, jc_json_t *sub_js)
{
    jc_key_t  *k;
    jc_val_t  *v;

    assert(key != NULL);
    assert(sub_js != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_JSON;
    v->data.j = sub_js;

    return jc_json_add_kv(js, k, v);
}

static size_t __jc_json_array_size(jc_array_t *arr)
{
    size_t   s, i;

    s = sizeof("[]") - 1;
    for (i = 0; i != arr->size; ++i) {
        s += __jc_json_val_size(arr->value[i]);
        if (i != arr->size - 1) {
            s += sizeof(",") - 1;
        }
    }
    return s;
}

static size_t __jc_json_val_size(jc_val_t *val)
{
    size_t s;

    s = 0;

    switch (val->type) {
        case JC_INT:
            /* max size int64_t: 0x8000000000000000 */
            s += sizeof("-9223372036854775808") - 1;
            break;
        case JC_BOOL:
            s += sizeof("false") - 1;
            break;
        case JC_STR:
            s += val->data.s->size + sizeof("\"\"") - 1;
            break;
        case JC_ARRAY:
            s += __jc_json_array_size(val->data.a);
            break;
        case JC_JSON:
            s += jc_json_size(val->data.j);
            break;
        case JC_NULL:
            s += sizeof("null") - 1;
            break;
        default:
            /* some error happends! */
            assert(0);
    }

    return s;
}

static size_t jc_json_size(jc_json_t *js)
{
    size_t  s, i;

    s = sizeof("{}");
    for (i = 0; i != js->size; ++i) {

        /* key size without teminating '\0' and "": */
        s += (js->keys[i]->size - 1) + (sizeof("\"\":") - 1);

        /* value size */
        s += __jc_json_val_size(js->vals[i]);

        if (i != js->size - 1) {
            /* add the length of separator of ',' */
            s += sizeof(",") - 1;
        }
    }
    return s;
}

static int __jc_json_value(jc_val_t *val, char *p)
{
    size_t       i;
    const char  *base;

    base = p;

    switch (val->type) {
        case JC_INT:
            p += sprintf(p, "%ld", val->data.i);
            break;

        case JC_BOOL:
            if (val->data.b) {
                p[0] = 't';
                p[1] = 'r';
                p[2] = 'u';
                p[3] = 'e';
                p += 4;
            } else {
                p[0] = 'f';
                p[1] = 'a';
                p[2] = 'l';
                p[3] = 's';
                p[4] = 'e';
                p += 5;
            }
            break;

        case JC_STR:
            p += sprintf(p, "\"%s\"", val->data.s->body);
            break;

        case JC_ARRAY:
            *p++ = '[';
            for (i = 0; i != val->data.a->size; ++i) {
                p += __jc_json_value(val->data.a->value[i], p);
                if (i != val->data.a->size - 1) {
                    *p++ = ',';
                }
            }
            *p++ = ']';
            break;

        case JC_JSON:
            p += __jc_json_str(val->data.j, p);
            break;

        case JC_NULL:
            p[0] = 'n';
            p[1] = 'u';
            p[2] = 'l';
            p[3] = 'l';
            p += 4;
            break;

        default:
            /* some error happens */
            assert(0);
    }

    return (int)(p - base);
}

static int __jc_json_str(jc_json_t *js, char *p)
{
    size_t       i;
    jc_val_t    *val;
    const char  *base;

    base = p;
    *p++ = '{';
    for (i = 0; i != js->size; ++i) {
        /* print key */
        p += sprintf(p, "\"%s\":", js->keys[i]->body);

        /* print value */
        p += __jc_json_value(js->vals[i], p);

        if (i != js->size - 1) {
            *p++ = ',';
        }
    }
    *p++ = '}';
    return (int)(p - base);
}
const char *jc_json_str(jc_json_t *js)
{
    size_t      jsize, n;
    char       *p;

    assert(js != NULL);

    jsize = jc_json_size(js);
    if ((p = jc_pool_alloc(js->pool, jsize + 1)) == NULL) {
        return NULL;
    }
    n = __jc_json_str(js, p);
    p[n] = '\0';

    return p;
}




