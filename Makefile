# Copyright (C) Teemu Suutari

VPATH	:= library testing

TARGET := Amiga

ifneq ($(TARGET), Amiga)
CC	= clang
CFLAGS	= -g -Wall -Werror -Ilibrary -I.
LDFLAGS =
INTEGRATION_OBJ = archivefs_integration_unix.o
LIB	=
else
AS	= vasmm68k_mot
CC	= vc
LD	= vc
CFLAGS	= -Ilibrary -I. -I$(INCLUDEOS3) -O2
AFLAGS	= -Fhunk -I$(INCLUDEOS3)
LDFLAGS =
INTEGRATION_OBJ = archivefs_integration_amiga.o
LIB	= WHDLoad.VFS
endif

PROG	= test

OBJS	= $(addprefix obj/,archivefs_api.o archivefs_common.o archivefs_lha.o archivefs_zip.o \
	archivefs_huffman_decoder.o archivefs_lha_decompressor.o)

OBJS_TEST = $(addprefix obj/,test.o CRC32.o $(INTEGRATION_OBJ)) $(OBJS)
OBJS_LIB = $(addprefix obj/,archivefs_header.o archivefs_integration_amiga_standalone.o) $(OBJS)

all: $(PROG) $(LIB)

obj:
	mkdir $@

obj/%.o: %.S | obj
	$(AS) $(AFLAGS) -o $@ $<

obj/archivefs_integration_amiga_standalone.o: archivefs_integration_amiga.c | obj
	 $(CC) $(CFLAGS) -o $@ -c $< -DARCHIVEFS_STANDALONE=1

ifeq ($(TARGET), Amiga)
obj/archivefs_huffman_decoder.o: CFLAGS+=-speed
obj/archivefs_lha_decompressor.o: CFLAGS+=-speed
endif
obj/%.o: %.c | obj
	$(CC) $(CFLAGS) -o $@ -c $<

$(PROG): $(OBJS_TEST)
	$(CC) $(LDFLAGS) -o $@ $^

$(LIB): $(OBJS_LIB)
	$(LD) -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel $^ -o $@ -lvc

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
