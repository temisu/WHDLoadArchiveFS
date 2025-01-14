# Copyright (C) Teemu Suutari

VPATH	:= library testing

# possible targets:
#	Amiga	build with vbcc
#	AmigaG	build with m68k-amigaos-gcc, requires https://github.com/bebbo/amiga-gcc
#	AmigaS	build with SAS/C, requires vamos (https://github.com/cnvogelg/amitools) on Linux/MacOS
#	*	build with clang for testing
TARGET	?= AmigaG

# set vamos if cross compile
ifneq ($(shell uname -s),AmigaOS)
VAMOS	= vamos -q --
endif

# because SAS/C has non standard arguments
CFLAGC	= -c
CFLAGD	= -D
CFLAGO	= -o
CFLAGP	= -o

ifeq (,$(findstring Amiga,$(TARGET)))
# non Amiga targets
CC	= clang
CFLAGS	= -g -Wall -Werror -Ilibrary -I.
LDFLAGS =
INTEGRATION_OBJ = archivefs_integration_unix.o
LIB	=
else
# Amiga* targets
# too lazy to construct AS for gcc/sas
AS	= vasmm68k_mot -Fhunk -I$(INCLUDEOS3) -quiet
ifeq ($(TARGET),Amiga)
CC	= vc
CFLAGS	= -Ilibrary -I. -I$(INCLUDEOS3) -sc -O2
CFSPEED	= -speed
CFLAGO	= -o 
CFLAGP	= -o 
LDFLAGS = -sc -final
MKLIB	= $(CC) $(LDFLAGS) -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel -lvc -lamiga $(CFLAGP)$@ $^
CCVER	= vbccm68k 2>/dev/null | awk '/vbcc V/ { printf " "$$1" "$$2 } /vbcc code/ { printf " "$$4" "$$5 }'
endif
ifeq ($(TARGET),AmigaG)
CC	= m68k-amigaos-gcc
CFLAGS	= -g -Wall -Ilibrary -I. -O2 -noixemul
CFSPEED	= -O3
LDFLAGS = -noixemul
MKLIB	= m68k-amigaos-ld $(CFLAGP)$@ $^ -lc --strip-all
CCVER	= m68k-amigaos-gcc --version | awk '/m68k/ { printf " "$$0 }'
endif
ifeq ($(TARGET),AmigaS)
CC	= $(VAMOS) sc
CFLAGS	= Data=FarOnly IdentifierLength=40 IncludeDirectory=library Optimize OptimizerSchedule NoStackCheck NoVersion
CFSPEED	= OptimizerTime OptimizerComplexity=10 OptimizerInLocal
CFLAGC	=
CFLAGD	= Define=
CFLAGO	= ObjectName=
CFLAGP	= ProgramName=
LDFLAGS = Link SmallData SmallCode
MKLIB	= $(VAMOS) slink SmallData SmallCode Quiet Lib lib:sc.lib To $@ From $^
CCVER	= $(VAMOS) sc | awk '/^SAS/ { printf " "$$0 }'
endif
INTEGRATION_OBJ = archivefs_integration_amiga.o
LIB	= WHDLoad.VFS

endif

PROG	= test

OBJS	= $(addprefix obj/,archivefs_api.o archivefs_common.o archivefs_lha.o archivefs_zip.o \
	archivefs_huffman_decoder.o archivefs_lha_decompressor.o archivefs_zip_decompressor.o)

OBJS_TEST = $(addprefix obj/,test.o CRC32.o $(INTEGRATION_OBJ)) $(OBJS)
OBJS_LIB = $(addprefix obj/,archivefs_header.o archivefs_integration_amiga_standalone.o) $(OBJS)

all: $(PROG) $(LIB)

obj:
	mkdir $@

obj/%.o: %.S .date .ccver | obj
	$(AS) -o $@ $<

obj/archivefs_integration_amiga_standalone.o: archivefs_integration_amiga.c | obj
	$(CC) $(CFLAGS) $(CFLAGO)$@ $(CFLAGC) $< $(CFLAGD)ARCHIVEFS_STANDALONE=1

obj/archivefs_huffman_decoder.o: CFLAGS+=$(CFSPEED)
obj/archivefs_lha_decompressor.o: CFLAGS+=$(CFSPEED)
obj/archivefs_zip_decompressor.o: CFLAGS+=$(CFSPEED)
obj/%.o: %.c | obj
	$(CC) $(CFLAGS) $(CFLAGO)$@ $(CFLAGC) $<

$(PROG): $(OBJS_TEST)
	$(CC) $(LDFLAGS) $(CFLAGP)$@ $^

$(LIB): $(OBJS_LIB)
	$(MKLIB)

.date:
	date "+(%d.%m.%Y)" | xargs printf > $@

.ccver:
	$(CCVER) > $@

.PHONY: all clean .date .ccver

clean:
	rm -f $(OBJS_LIB) $(OBJS_TEST) $(PROG) $(LIB) *~ */*~ .date .ccver *.lnk
	test -d obj && rmdir obj || true

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
	@./testing/run_test.sh testing/test13.txt
	@./testing/run_test.sh testing/test14.txt
	@./testing/run_test.sh testing/test15.txt
	@./testing/run_test.sh testing/test16.txt
	@./testing/run_test.sh testing/test17.txt
	@./testing/run_test.sh testing/test18.txt
	@./testing/run_test.sh testing/test19.txt
	@./testing/run_test.sh testing/test20.txt
	@./testing/run_test.sh testing/test21.txt
	@./testing/run_test.sh testing/test22.txt

