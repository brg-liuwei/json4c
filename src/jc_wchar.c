#include "jc_wchar.h"

#include <string.h>

static int hex_table[256];
static int jc_wc_init(const char *unicode, char *chs);
static int jc_wctomb_interal(const char *unicode, char *chs);

static int (*jc_wctomb_func)(const char *, char *) = jc_wc_init;

static int ch2hex(char ch)
{
    return hex_table[(unsigned char)ch];
}

static int jc_wc_init(const char *unicode, char *chs)
{
    char  ch;

    setlocale(LC_ALL, "");
    memset(hex_table, -1, sizeof hex_table);
    for (ch = '0'; ch <= '9'; ++ch) {
        hex_table[ch] = ch - '0';
    }
    for (ch = 'a'; ch <= 'f'; ++ch) {
        hex_table[ch] = ch - 'a' + 10;
    }
    for (ch = 'A'; ch <= 'F'; ++ch) {
        hex_table[ch] = ch - 'A' + 10;
    }

    jc_wctomb_func = jc_wctomb_interal;

    return jc_wctomb_interal(unicode, chs);
}

static int jc_wctomb_interal(const char *unicode, char *chs)
{
    int      i, num;
    char    *p;
    char     buf[16];
    wchar_t  wc;

    p = (char *)unicode;
    if (p[0] != '\\' || p[1] != 'u') {
        return -1;
    }
    for (p = (char *)&unicode[2], i = 0, wc = 0; i != 4; ++i) {
        num = ch2hex(p[i]);
        if (num == -1) {
            return -1;
        }
        wc <<= 4; /* wc *= 16 */
        wc += 0xF & (wchar_t)num;
    }
    if (chs != NULL) {
        p = chs;
    } else {
        p = buf;
    }
    return wctomb(p, wc);
}

int jc_wctomb(const char *unicode, char *chs)
{
    return jc_wctomb_func(unicode, chs);
}
