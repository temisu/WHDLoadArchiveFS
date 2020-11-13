# Copyright (C) Teemu Suutari

VPATH	:= testing

#TARGET := Amiga

ifneq ($(TARGET), Amiga)
CC	= clang
CFLAGS	= -g -Wall -Werror -I.
LDFLAGS =
INTEGRATION_OBJ = container_integration_unix.o
else
export PATH := $(PATH):$(HOME)/vbcc_cross
CC	= $(HOME)/vbcc_cross/vc
CFLAGS	= -I.
LDFLAGS =
INTEGRATION_OBJ = container_integration_amiga.o
endif

PROG	= test

OBJS	= container_api.o container_common.o container_lha.o container_zip.o $(INTEGRATION_OBJ) \
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
