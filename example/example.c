#include "jc_type.h"

#include <stdio.h>
#include <string.h>

/*
 * just to test
 * */

#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUEBG "\033[44m"
#define WHITEBG "\033[47m"
#define BLINK "\033[5m"
#define HIGHLIGHT "\033[1m"
#define NONE "\033[0m"

int main(int argc, char *argv[])
{
    int         i;
    jc_json_t  *js, *top, *js_pool[10];

    top = jc_json_create();
    for (i = 0; i != 10; ++i) {
        js = jc_json_create();
        js_pool[i] = js;

        jc_json_add_bool(js, "iOS", 0);
        jc_json_add_num(js, "Windows", 18);
        jc_json_add_num(js, "Sina", -3.88);
        jc_json_add_num(js, "Tencent", 100.2e+9);
        jc_json_add_str(js, "Linux", "bridge");
        jc_json_add_str(js, "Linux", "brg-liuwei");
        jc_json_add_str(js, "Linux", "trump");
        jc_json_add_array(js, "Andriod");
        jc_json_add_str(js, "Andriod", "Alibaba");
        jc_json_add_null(js, "Sohu");

        jc_json_add_json(top, "Google", js);
    }

    jc_json_add_str(js, "360", "Beat Baidu!");
    jc_json_add_str(top, "Zoo", "Hadoop");
    jc_json_add_str(top, "Zoo", "Cassandra");
    jc_json_add_str(top, "Zoo", "Hive");

    printf(RED WHITEBG HIGHLIGHT "%s" NONE "\n", jc_json_str(top));

    jc_json_destroy(top);
    for (i = 0; i != 10; ++i) {
        jc_json_destroy(js_pool[i]);
    }

    char *json_str = "{\"RodWell\":\"America\",\"Fatoni\":18e-3,"
        "\"Mekstroth\":-8.88,\"Belladona\":-98e-2,"
        "\"Reese\":99.999e+3,\"Zia\":\"America\","
        "\"Gorozzo\":\"Italy\",\"Hanman\":\"America\","
        "\"Nunes\":\"Italy\",\"Dijver\":\"Netherland\",\"Boll\":null,"
        "\"Gestem\":true,\"Gallizzi\":true,\"Stayman\":false,"
        "\"Aunicode\":\"\u4e2d\u56fd\"}";
    char top_str[2048];
    snprintf(top_str, 2048, "{\"json\":{\"a\":[%s,%s,%s]},\"hello\":%s,\"array\":[%s]}", json_str, json_str, json_str, json_str, json_str);
    top = jc_json_parse(top_str);
    if (top) {
        printf(YELLOW HIGHLIGHT BLUEBG "%s" NONE "\n", jc_json_str(top));
    } else {
        printf(YELLOW HIGHLIGHT BLUEBG "%s" NONE "\n", top_str);
    }
    jc_json_destroy(top);

    jc_json_t *js2;
    const char *s = "{\"title\":\"双引号测试：\\\"中华人民共和国\\\"\",\"\\\"双引号\\\"key测试\":true,\"斜杠测试\":\"///\"}";
    printf("%s\n", s);

    js2 = jc_json_parse(s);
    printf("%s\n", jc_json_str(js2));
    jc_json_destroy(js2);

    js2 = jc_json_parse(s);
    const char *js2_str = jc_json_str(js2);
    jc_json_destroy(js2);

    js2 = jc_json_parse(js2_str);
    printf("%s\n", jc_json_str(js2));
    jc_json_destroy(js2);

    return 0;
}

