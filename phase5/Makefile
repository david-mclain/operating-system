PREFIX = ..

CC = gcc

CSRCS = $(wildcard *.c)
COBJS = $(CSRCS:.c=.o)

LIBS = -lusloss4.7 -lphase1 -lphase2 -lphase3 -lphase4

LIB_DIR     = ${PREFIX}/lib
INCLUDE_DIR = ${PREFIX}/include

CFLAGS = -Wall -g -I${INCLUDE_DIR} -I.
LDFLAGS = -Wl,--start-group -L${LIB_DIR} -L. ${LIBS} -Wl,--end-group



VPATH = testcases
TESTS = test00 test01 test02 test03 test04 test05 test06 test07 test08 test09 \
        test10



all: ${TESTS}

${TESTS}: phase5_common_testcase_code.o $(COBJS) libphase1.a libphase2.a libphase3.a libphase4.a

ARCH=$(shell uname | tr '[:upper:]' '[:lower:]')-$(shell uname -p | sed -e "s/aarch/arm/g")

phase5_no_debug_symbols-${ARCH}.o: phase5.c
	gcc -I${INCLUDE_DIR} -I. -c phase5.c -o phase5_no_debug_symbols-${ARCH}.o

libphase5-${ARCH}.a: phase5_no_debug_symbols-${ARCH}.o
	-rm $@
	ar -r $@ $^

clean:
	-rm *.o ${TESTS} term[0-3].out

