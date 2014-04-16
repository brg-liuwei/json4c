#include "jc_alloc.h"
#include "jc_type.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int          i;
    char        *p1, *p2, *p3;
    jc_pool_t   *pool;

    //pool = jc_pool_create(100);
    //p1 = jc_pool_alloc(pool, sizeof("first"));
    //strcpy(p1, "first");
    //p2 = jc_pool_alloc(pool, sizeof("second"));
    //strcpy(p2, "second");
    //p3 = jc_pool_alloc(pool, sizeof("third"));
    //strcpy(p3, "third");

    //for (i = 0; i != 1000; ++i) {
    //    jc_pool_alloc(pool, i * 10);
    //}

    //printf("%s, %s, %s\n", p1, p2, p3);
    //jc_pool_destroy(pool);

    jc_json_t  *js, *top;


    for ( ; ; ) {
        top = jc_json_create();

        for (i = 0; i != 10; ++i) {
            js = jc_json_create();

            jc_json_add_bool(js, "iOS", 0);
            jc_json_add_int(js, "Windows", 18);
            jc_json_add_str(js, "Linux", "bridge");
            jc_json_add_str(js, "Linux", "brg-liuwei");
            jc_json_add_str(js, "Linux", "Shoffing");
            jc_json_add_array(js, "Andriod");
            jc_json_add_str(js, "Andriod", "QingTing");
            jc_json_add_null(js, "Sohu");

            jc_json_add_json(top, "Google", js);
        }

        jc_json_add_str(js, "360", "Beat Baidu!");
        jc_json_add_str(top, "Zoo", "Hadoop");
        jc_json_add_str(top, "Zoo", "Cassandra");
        jc_json_add_str(top, "Zoo", "Hive");

        //printf("%s\n", jc_json_str(top));

        jc_json_destroy(top);
    }

    return 0;
}

