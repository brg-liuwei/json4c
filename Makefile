CC ?= gcc
RM ?= RM -rf
OBJS = src/jc_alloc.o src/jc_type.o src/jc_wchar.o

EXAMPLE_OBJS = example/example.o
EXAMPLE_BIN = example/example

CONF_H = jc_config.h
$(shell ./build_conf.sh ${CONF_H})

IFLAGS = -I. -I./src

LIB = libjson4c.a

.PHONY: all
all: ${LIB}
${LIB}: ${OBJS}
	${AR} -rs $@ ${OBJS}
${OBJS}: %.o: %.c
	${CC} ${IFLAGS} -c $^ -o $@

.PHONY: example
example: ${EXAMPLE_BIN}
${EXAMPLE_BIN}: ${EXAMPLE_OBJS} ${OBJS}
	${CC} ${IFLAGS} -o $@ $^
${EXAMPLE_OBJS}: %.o: %.c
	${CC} ${IFLAGS} -c $^ -o $@

.PHONY: clean
clean:
	${RM} ${CONF_H} ${OBJS} ${EXAMPLE_BIN} ${EXAMPLE_OBJS} ${LIB}
