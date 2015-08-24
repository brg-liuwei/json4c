#!/bin/bash

test_header() {
    gcc -x c - -o /dev/null 2>/dev/null << EOF
        #include <$1>
        int main() { return 0; }
EOF
}

CONF=$1
VAR=$2
if test -z "$CONF" | test -z "$VAR"; then
    echo "Usage: $0 <config file> <var file>"
    echo "eg:"
    echo "    $0 config.h var"
    exit 1
fi

# remove old records
rm -rf $CONF
touch $CONF
rm -rf $VAR
touch $VAR

if test -z "$CC"; then
    CC=gcc
fi

echo "#ifndef __JC_CONFIG_H__" >> $CONF
echo "#define __JC_CONFIG_H__" >> $CONF
echo "" >> $CONF

$(test_header malloc.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_MALLOC_H" >> $CONF
fi

$(test_header stdlib.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_STDLIB_H" >> $CONF
fi

$(test_header sys/types.h)
if [ "$?" == 0 ]; then
    echo "#define HAVE_SYS_TYPES_H" >> $CONF
fi

echo "" >> $CONF
echo "#endif" >> $CONF

os=$(uname -s)
if [ $os == "Darwin" ]; then
    # mac os
    #echo "DYNAMIC_LIB=libjson4c.dylib" >> $VAR
    echo "DYLIB_SUFFIX=.dylib" >> $VAR
elif [ $os == "Linux" ]; then
    # linux
    #echo "DYNAMIC_LIB=libjson4c.so" >> $VAR
    echo "DYLIB_SUFFIX=.so" >> $VAR
else
    echo "Unknown OS: " $os
    exit 1
fi
