COBJECT=jc_alloc.o jc_type.o jc_main.o
CC=gcc -g
#FLAG=-DHAVE_MALLOC_H
FLAG=-DHAVE_STDLIB_H
DEBUG=

all: out
$(COBJECT):%.o:%.c
	$(CC) -I. $(FLAG) $(DEBUG) -c $< -o $@
out:$(COBJECT)
	$(CC) -o out ${COBJECT}

clean:
	$(RM) *.o out
