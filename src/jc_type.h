#ifndef __JC_TYPE_H__
#define __JC_TYPE_H__

#include <sys/types.h>
#include <stdint.h>

#include "jc_alloc.h"

typedef struct jc_json_s     jc_json_t;

/* json create and delete functions */
jc_json_t *jc_json_create();
jc_json_t *jc_json_parse(const char *json_str);
void jc_json_destroy(jc_json_t *js);

/* json add kv functions */
int jc_json_add_bool(jc_json_t *js, const char *key, int bool_val);
int jc_json_add_num(jc_json_t *js, const char *key, double val);
int jc_json_add_str(jc_json_t *js, const char *key, const char *val);
int jc_json_add_array(jc_json_t *js, const char *key);
int jc_json_add_json(jc_json_t *js, const char *key, jc_json_t *sub_js);
int jc_json_add_null(jc_json_t *js, const char *key);

/* json to string function */
const char *jc_json_str(jc_json_t *js);

#endif

