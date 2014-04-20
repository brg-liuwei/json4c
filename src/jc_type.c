#include "jc_type.h"
#include "jc_alloc.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define JC_MEMSIZE 1024
#define JC_INCSTEP 16

typedef short                jc_bool_t;
typedef double               jc_num_t;
typedef struct jc_str_s      jc_str_t;
typedef struct jc_array_s    jc_array_t;

typedef struct jc_str_s      jc_key_t;
typedef struct jc_val_s      jc_val_t;

typedef enum __jc_type_t {
    JC_BOOL = 0,
    JC_NUM,
    JC_STR,
    JC_ARRAY,
    JC_JSON,
    JC_NULL
} jc_type_t;

struct jc_str_s {
    size_t   size;     /* size of str */
    size_t   free;     /* free space */
    char     body[];   /* str */
};

struct jc_array_s {
    size_t      size;     /* length of array */
    size_t      free;     /* free size of array */
    jc_val_t  **value;
};

struct jc_val_s {
    jc_type_t         type;
    union {
        jc_bool_t     b;
        jc_num_t      n;
        jc_str_t     *s;
        jc_array_t   *a;
        jc_json_t    *j;
    } data;
};

struct jc_json_s {
    size_t       size;     /* size of keys and values */
    size_t       free;     /* free size of keys and values */
    jc_key_t   **keys;     /* keys of json */
    jc_val_t   **vals;     /* values of json */
    jc_pool_t   *pool;     /* mem pool of json */
};

static int __jc_json_str(jc_json_t *js, char *p);
static size_t __jc_json_val_size(jc_val_t *val);
static size_t __jc_json_size(jc_json_t *js);
static int __jc_json_parse_key(jc_json_t *js, const char *p, jc_key_t **key);

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

    if (js == NULL) {
        return;
    }

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

int jc_json_add_num(jc_json_t *js, const char *key, double n)
{
    jc_key_t  *k;
    jc_val_t  *v;
    assert(key != NULL);

    if ((k = jc_key(js->pool, key)) == NULL) {
        return -1;
    }
    v = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    v->type = JC_NUM;
    v->data.n = n;

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
    char f[512];

    s = 0;

    switch (val->type) {
        case JC_BOOL:
            s += sizeof("false") - 1;
            break;
        case JC_NUM:
            /* use snprintf to calc the size of double */
            //s += snprintf(f, 512, "%.03lg", val->data.n);
            s += snprintf(f, 512, "%.03lg", val->data.n);
            break;
        case JC_STR:
            s += val->data.s->size + sizeof("\"\"") - 1;
            break;
        case JC_ARRAY:
            s += __jc_json_array_size(val->data.a);
            break;
        case JC_JSON:
            s += __jc_json_size(val->data.j);
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

static size_t __jc_json_size(jc_json_t *js)
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

        case JC_NUM:
            p += sprintf(p, "%.03lg", val->data.n);
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

    jsize = __jc_json_size(js);
    if ((p = jc_pool_alloc(js->pool, jsize + 1)) == NULL) {
        return NULL;
    }
    n = __jc_json_str(js, p);
    p[n] = '\0';

    return p;
}

/* ====================================
 * To parse json string to json object, 
 * We use state mechine. For detail,
 * please see http://www.json.org/ 
 * ==================================== */

typedef enum {
    JC_NUM_START_SIGN = 0,
    JC_NUM_START_ZERO,
    JC_NUM_START_DIG,
    JC_NUM_INT_DIG,
    JC_NUM_POINT,
    JC_NUM_FLT_DIG,
    JC_NUM_E_SYMBOL,
    JC_NUM_E_SIGN,
    JC_NUM_E_DIG
} jc_num_state_t;

static int __jc_json_parse_number(jc_json_t *js, const char *p, jc_val_t **val)
{
    int      n, sign, e_sign, base;
    int64_t  i_part, e_part;
    double   f_tmp, f_factor, f_part;
    double   f_val;

    jc_num_state_t  state;

    sign = e_sign = 0;
    i_part = e_part = 0;
    f_part = 0;
    f_factor = 1;

    if (p[0] == '-') {
        /* if p[0] == '+': illegal */
        state = JC_NUM_START_SIGN;
        sign = 1;
    } else if (p[0] == '0') {
        state = JC_NUM_START_ZERO;
    } else if (p[0] >= '1' && p[0] <= '9') {
        state = JC_NUM_START_DIG;
        i_part = p[0] - '0';
    } else {
        return -1;
    }

    for (n = 1; p[n] != '\0'; ++n) {
        switch (state) {
            case JC_NUM_START_SIGN:
                if (p[n] == '0') {
                    state = JC_NUM_START_ZERO;
                } else if (p[n] >= '1' && p[n] <= '9') {
                    state = JC_NUM_START_DIG;
                    i_part = p[n] - '0';
                } else {
                    return -1;
                }
                break;

            case JC_NUM_START_ZERO:
                if (p[n] == '.') {
                    state = JC_NUM_POINT;
                } else if (p[n] == 'e' || p[n] == 'E') {
                    state = JC_NUM_E_SYMBOL;
                } else {
                    goto calc;
                }
                break;

            case JC_NUM_START_DIG:
                if (p[n] >= '0' && p[n] <= '9') {
                    state = JC_NUM_INT_DIG;
                    i_part *= 10;
                    i_part += p[n] - '0';
                } else if (p[n] == '.') {
                    state = JC_NUM_POINT;
                } else if (p[n] == 'e' || p[n] == 'E') {
                    state = JC_NUM_E_SYMBOL;
                } else {
                    goto calc;
                }
                break;

            case JC_NUM_INT_DIG:
                if (p[n] >= '0' && p[n] <= '9') {
                    i_part *= 10;
                    i_part += p[n] - '0';
                } else if (p[n] == '.') {
                    state = JC_NUM_POINT;
                } else if (p[n] == 'e' || p[n] == 'E') {
                    state = JC_NUM_E_SYMBOL;
                } else {
                    goto calc;
                }
                break;

            case JC_NUM_POINT:
                if (p[n] >= '0' && p[n] <= '9') {
                    f_factor *= 0.1;
                    f_tmp = f_factor * (p[n] - '0');
                    f_part += f_tmp;
                    state = JC_NUM_FLT_DIG;
                } else {
                    return -1;
                }
                break;

            case JC_NUM_FLT_DIG:
                if (p[n] >= '0' && p[n] <= '9') {
                    f_factor *= 0.1;
                    f_tmp = f_factor * (p[n] - '0');
                    f_part += f_tmp;
                    state = JC_NUM_FLT_DIG;
                } else if (p[n] == 'e' || p[n] == 'E') {
                    state = JC_NUM_E_SYMBOL;
                } else {
                    goto calc;
                }
                break;

            case JC_NUM_E_SYMBOL:
                if (p[n] == '+') {
                    state = JC_NUM_E_SIGN;
                } else if (p[n] == '-') {
                    e_sign = 1;
                    state = JC_NUM_E_SIGN;
                } else if (p[n] >= '0' && p[n] <= '9') {
                    e_part *= 10;
                    e_part += (p[n] - '0');
                    state = JC_NUM_E_DIG;
                } else {
                    return -1;
                }
                break;

            case JC_NUM_E_SIGN:
                if (p[n] >= '0' && p[n] <= '9') {
                    e_part *= 10;
                    e_part += (p[n] - '0');
                    state = JC_NUM_E_DIG;
                } else {
                    return -1;
                }
                break;

            case JC_NUM_E_DIG:
                if (p[n] >= '0' && p[n] <= '9') {
                    e_part *= 10;
                    e_part += (p[n] - '0');
                } else {
                    goto calc;
                }
                break;
        }
    }

calc:
    *val = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    if (*val == NULL) {
        return -1;
    }

    /* double */
    f_val = i_part + f_part;
    if (sign != 0) {
        f_val = -f_val;
    }

    for (f_factor = 1, base = 1; e_part != 0; e_part >>= 1) {
        base *= 10;
        if (e_part & 0x1) {
            f_factor *= base;
        }
    }

    (*val)->type = JC_NUM;
    (*val)->data.n = (e_sign == 0 ? f_val * f_factor : f_val / f_factor);
    return n;
}

static int __jc_json_parse_bool(jc_json_t *js, const char *p, jc_val_t **val)
{
    jc_bool_t  b;

    if (p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e') {
        b = 1;
    } else if (p[0] == 'f' && p[1] == 'a' && p[2] == 'l' 
            && p[3] == 's' && p[4] == 'e')
    {
        b = 0;
    } else {
        return -1;
    }

    *val = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    if (*val == NULL) {
        return -1;
    }
    (*val)->type = JC_BOOL;
    (*val)->data.b = b;
    return b == 1 ? 4 : 5;
}

static int __jc_json_parse_null(jc_json_t *js, const char *p, jc_val_t **val)
{
    if (p[0] == 'n' && p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
        *val = jc_pool_alloc(js->pool, sizeof(jc_val_t));
        if (*val == NULL) {
            return -1;
        }
        (*val)->type = JC_NULL;
        return 4;   /* strlen("null") */
    }
    return -1;
}

static int __jc_json_parse_str(jc_json_t *js, const char *p, jc_val_t **val)
{
    int         n;
    jc_str_t   *str;

    n = __jc_json_parse_key(js, p, &str);
    if (n < 0) {
        return -1;
    }
    *val = jc_pool_alloc(js->pool, sizeof(jc_val_t));
    if (*val == NULL) {
        return -1;
    }
    (*val)->type = JC_STR;
    (*val)->data.s = str;
    return n;
}

static int __jc_json_parse_array(jc_json_t *js, const char *p, jc_val_t **val) { return -1; }

static int __jc_json_parse_sub_json(jc_json_t *js, const char *p, jc_val_t **val) { return -1; }

typedef enum {
    JC_STR_START = 0,
    JC_STR_START_QUA,
    JC_STR_CHR,
    JC_STR_TRANS,        /* reverse solidus */
    JC_STR_END_QUA,
} jc_str_state_t;

static int __jc_json_parse_key(jc_json_t *js, const char *p, jc_key_t **key)
{
    size_t   n;
    size_t   size;

    jc_str_state_t  state;

    if (p[0] != '\"') {
        return -1;
    }
    for (n = 1, size = 0; /* void */ ; /* void */ ) {
        switch (p[n]) {
            case '\0':
                return -1;
            case '\\':
                if (p[n+1] == '\0') {
                    return -1;
                }
                /* TODO: need to judge size of Unicode if support \uXXXX */
                /* if (p[n+1] == 'u') {
                 *    calc size
                 * }
                 * */
                n += 2;
                break;
            case '"':
                goto parse;
            default:
                ++n;
                ++size;
        }
    }

parse:
    size = jc_align(sizeof(jc_key_t) + size + 1);  /* add the terminating zero */
    if ((*key = jc_pool_alloc(js->pool, size)) == NULL) {
        return -1;
    }
    (*key)->size = 0;
    (*key)->free = size - sizeof(jc_key_t);

    for (n = 0, state = JC_STR_START, size = 0; p[n] != '\0'; /* void */ ) {
        switch (state) {
            case JC_STR_START:
                if (p[n++] != '\"') {
                    return -1;
                }
                state = JC_STR_START_QUA;
                break;

            case JC_STR_START_QUA:
                if (p[n] == '\"') {
                    state = JC_STR_END_QUA;
                } else if (p[n] == '\\') {
                    state = JC_STR_TRANS;
                } else {
                    state = JC_STR_CHR;
                    (*key)->body[size++] = p[n];
                }
                ++n;
                break;

            case JC_STR_CHR:
                if (p[n] == '\"') {
                    state = JC_STR_END_QUA;
                } else if (p[n] == '\\') {
                    state = JC_STR_TRANS;
                } else {
                    (*key)->body[size++] = p[n];
                }
                ++n;
                break;

            case JC_STR_TRANS:
                switch (p[n]) {
                    case '\"':
                        (*key)->body[size++] = '\"';
                        break;
                    case '\\':
                        (*key)->body[size++] = '\\';
                        break;
                    case '/':
                        (*key)->body[size++] = '/';
                        break;
                    case 'b':
                        (*key)->body[size++] = '\b';
                        break;
                    case 'f':
                        (*key)->body[size++] = '\f';
                        break;
                    case 'n':
                        (*key)->body[size++] = '\n';
                        break;
                    case 'r':
                        (*key)->body[size++] = '\r';
                        break;
                    case 't':
                        (*key)->body[size++] = '\t';
                        break;
                    case 'u':
                        /* TODO: use wctomb to trans Unicode to Chars */
                        return -1;
                    default:
                        /* illegal char: \x */
                        return -1;
                }
                ++n;
                state = JC_STR_CHR;
                break;

            case JC_STR_END_QUA:
                (*key)->body[size++] = '\0';
                assert(size <= (*key)->free);
                (*key)->free -= size;
                (*key)->size = size;
                return n;
        }
    }
    return -1;
}

static int __jc_json_parse_val(jc_json_t *js, const char *p, jc_val_t **val)
{
    switch (*p) {
        case '\"':
            return __jc_json_parse_str(js, p, val);
        case '[':
            return __jc_json_parse_array(js, p, val);
        case '{':
            return __jc_json_parse_sub_json(js, p, val);
        case 't':
        case 'f':
            return __jc_json_parse_bool(js, p, val);
        case 'n':
            return __jc_json_parse_null(js, p, val);
        default:
            if ((*p >= '0' && *p <= '9') || (*p == '-')) {
                return __jc_json_parse_number(js, p, val);
            }
    }
    *val = NULL;
    return -1;
}

typedef enum {
    JC_OBJ_START = 0,
    JC_OBJ_LBRACE,
    JC_OBJ_KEY,
    JC_OBJ_VAL,
    JC_OBJ_COLON,
} jc_obj_state_t;

jc_json_t *jc_json_parse(const char *p)
{
    int           n;
    jc_key_t     *key;
    jc_val_t     *val;
    jc_json_t    *js;
    jc_obj_state_t  state;

    assert(p != NULL);
    if ((js = jc_json_create()) == NULL) {
        return NULL;
    }

    state = JC_OBJ_START;
    while (*p != '\0') {
        switch (state) {
            case JC_OBJ_START:
                if (*p++ == '{') {
                    state = JC_OBJ_LBRACE;
                    break;
                }
                goto error;

            case JC_OBJ_LBRACE:
            case JC_OBJ_KEY:
                n = __jc_json_parse_key(js, p, &key);
                if (n > 0) {
                    p += n;
                    state = JC_OBJ_COLON;
                    break;
                }
                goto error;

            case JC_OBJ_COLON:
                if (*p++ == ':') {
                    state = JC_OBJ_VAL;
                    break;
                }
                goto error;

            case JC_OBJ_VAL:
                n = __jc_json_parse_val(js, p, &val);
                if (n > 0) {
                    p += n;
                    if (*p == ',') {
                        /* in JC_OBJ_COMMA state */
                        state = JC_OBJ_KEY;
                    } else if (*p == '}') {
                        /* Accept */
                        jc_json_add_kv(js, key, val);
                        return js;
                    } else {
                        goto error;
                    }
                    ++p;
                    jc_json_add_kv(js, key, val);
                    break;
                }
                goto error;

            default:
                /* some error happens */
                assert(0);
        }
    }

error:
    jc_json_destroy(js);
    return NULL;
}

