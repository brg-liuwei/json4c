CC ?= gcc
RM ?= RM -rf
OBJS = src/jc_alloc.o src/jc_type.o src/jc_wchar.o

EXAMPLE_OBJS = example/example.o
EXAMPLE_BIN = example/example

CONF_H = jc_config.h
VAR = vars.mk
$(shell ./build_conf.sh ${CONF_H} ${VAR})
include ${VAR}

IFLAGS = -I. -I./include/json4c
CFLAGS = -fPIC

STATIC_LIB = libjson4c.a
DYNAMIC_LIB = libjson4c${DYLIB_SUFFIX}

.PHONY: all
all: ${STATIC_LIB} ${DYNAMIC_LIB}
${STATIC_LIB}: ${OBJS}
	${AR} -rs $@ ${OBJS}
${DYNAMIC_LIB}: ${OBJS}
	${CC} -shared ${OBJS} -o $@
${OBJS}: %.o: %.c
	${CC} ${IFLAGS} ${CFLAGS} -c $^ -o $@

.PHONY: example
example: ${EXAMPLE_BIN}
${EXAMPLE_BIN}: ${EXAMPLE_OBJS} ${OBJS}
	${CC} ${IFLAGS} -o $@ $^
${EXAMPLE_OBJS}: %.o: %.c
	${CC} ${IFLAGS} -c $^ -o $@

.PHONY: clean
clean:
	${RM} ${CONF_H} ${VAR} ${OBJS} ${EXAMPLE_BIN} ${EXAMPLE_OBJS} ${STATIC_LIB} ${DYNAMIC_LIB}
