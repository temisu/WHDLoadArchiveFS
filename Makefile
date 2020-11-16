# Copyright (C) Teemu Suutari

VPATH	:= testing

TARGET := Amiga

ifneq ($(TARGET), Amiga)
CC	= clang
CFLAGS	= -g -Wall -Werror -I.
LDFLAGS =
INTEGRATION_OBJ = archivefs_integration_unix.o
LIB	=
else
export PATH := $(PATH):$(HOME)/vbcc_cross
AS	= $(HOME)/vbcc_cross/vasmm68k_mot
CC	= $(HOME)/vbcc_cross/vc
LD	= $(HOME)/vbcc_cross/vlink
CFLAGS	= -I.
AFLAGS	= -Fhunk -Itarget/include_i 
LDFLAGS =
INTEGRATION_OBJ = archivefs_integration_amiga.o
LIB	= ArchiveFS.whdvfs
endif

PROG	= test

OBJS	= archivefs_api.o archivefs_common.o archivefs_lha.o archivefs_zip.o

OBJS_TEST = test.o CRC32.o $(INTEGRATION_OBJ) $(OBJS)
OBJS_LIB = archivefs_header.o archivefs_integration_amiga_standalone.o $(OBJS)

all: $(PROG) $(LIB)

%.o: %.S
	$(AS) $(AFLAGS) -o $@ $<

archivefs_integration_amiga_standalone.o: archivefs_integration_amiga.c
	 $(CC) $(CFLAGS) -o $@ -c $< -DARCHIVEFS_STANDALONE=1

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(PROG): $(OBJS_TEST)
	$(CC) $(LDFLAGS) -o $@ $^

$(LIB): $(OBJS_LIB)
	$(LD) -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel $^ -o $@ -Ltarget/lib -lvc

clean:
	rm -f $(OBJS_LIB) $(OBJS_TEST) $(PROG) $(LIB) *~ */*~

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
