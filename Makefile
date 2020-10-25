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
	@./testing/run_test.sh testing/test5.txt
	@./testing/run_test.sh testing/test6.txt
	@./testing/run_test.sh testing/test7.txt
	@./testing/run_test.sh testing/test8.txt
	@./testing/run_test.sh testing/test9.txt
	@./testing/run_test.sh testing/test10.txt
	@./testing/run_test.sh testing/test11.txt
	@./testing/run_test.sh testing/test12.txt
