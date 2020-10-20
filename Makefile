# Copyright (C) Teemu Suutari

VPATH	:= testing

CC	= clang
CFLAGS	= -g -Wall -Werror -I.
LDFLAGS =

PROG	= test

OBJS	= container_api.o container_common.o container_integration.o container_lha.o container_zip.o \
	test.o CRC32.o

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

clean:
	rm -f $(OBJS) $(PROG) *~ */*~

run_tests: $(PROG)
	@./testing/run_test.sh testing/test1.txt
	@./testing/run_test.sh testing/test2.txt
	@./testing/run_test.sh testing/test3.txt
	@./testing/run_test.sh testing/test4.txt
