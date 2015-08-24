#!/bin/sh

test_header() {
    gcc -x c - -o /dev/null 2>/dev/null << EOF
        #include <$1>
        int main() { return 0; }
EOF
}

OUTPUT=$1
if test -z "$OUTPUT"; then
    echo "Usage: $0 <output file>"
    echo "eg:"
    echo "    $0 config.h"
    exit 1
fi

# remove old records
rm -rf $OUTPUT
touch $OUTPUT

if test -z "$CC"; then
    CC=gcc
fi

if [ ! -d "lib" ]; then
    $(mkdir lib)
fi

echo "#ifndef __JC_CONFIG_H__" >> $OUTPUT
echo "#define __JC_CONFIG_H__" >> $OUTPUT
echo "" >> $OUTPUT

$(test_header malloc.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_MALLOC_H" >> $OUTPUT
fi

$(test_header stdlib.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_STDLIB_H" >> $OUTPUT
fi

$(test_header sys/types.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_SYS_TYPES_H" >> $OUTPUT
fi

echo "" >> $OUTPUT
echo "#endif" >> $OUTPUT
