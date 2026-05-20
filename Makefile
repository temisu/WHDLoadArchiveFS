# Copyright (C) Teemu Suutari
# TARGET=Amiga|AmigaG|AmigaS|clang selects compiler
# PROF=1 compiles with profiling support (clang/gcc)
#	m68k-amigaos-gprof test
#	m68k-amigaos-gprof test | gprof2dot  | dot -Tpdf -o output.pdf

VPATH	:= library testing

# possible targets:
#	Amiga	build with vbcc
#	AmigaG	build with m68k-amigaos-gcc, requires https://github.com/bebbo/amiga-gcc
#	AmigaS	build with SAS/C, requires vamos (https://github.com/cnvogelg/amitools) on Linux/MacOS
#	*	build with clang for testing
TARGET	?= AmigaG
CPU	= 68000

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
ifeq ($(PROF),1)
CFLAGS	+= -pg
LDFLAGS += -pg
endif
else
# Amiga* targets
# too lazy to construct AS for gcc/sas
AS	= vasmm68k_mot -Fhunk -I$(INCLUDEOS3) -quiet -wfail -opt-allbra -opt-clr -opt-lsl -opt-nmoveq -opt-pea -m$(CPU)
INTEGRATION_OBJ = archivefs_integration_amiga.o

ifeq ($(TARGET),AmigaV)
CC	= vc
CFLAGS	= -Ilibrary -I. -I$(INCLUDEOS3) -sc -O2 -cpu=$(CPU)
CFSPEED	= -speed
CFLAGO	= -o 
CFLAGP	= -o 
LDFLAGS = -sc -final
MKLIB	= $(CC) $(LDFLAGS) -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel -lvc -lamiga $(CFLAGP)$@ $^
CCVER	= vbccm68k 2>/dev/null | awk '/vbcc V/ { printf " "$$1" "$$2 } /vbcc code/ { printf " "$$4" "$$5 }'
endif

ifeq ($(TARGET),AmigaG)
CC	= m68k-amigaos-gcc
CFLAGS	= -g -Wall -Ilibrary -I. -O2 -noixemul -m$(CPU)
CFSPEED	= -O3
LDFLAGS = -noixemul
MKLIB	= m68k-amigaos-ld -o $@ $^ -lc --strip-all
CCVER	= m68k-amigaos-gcc --version | awk '/m68k/ { printf " "$$0 }'
ifeq ($(PROF),1)
CFLAGS	+= -pg
LDFLAGS += -pg
# building library with profiling does not work
_PROF_NOLIB = 1
endif
endif

ifeq ($(TARGET),AmigaS)
CC	= $(VAMOS) sc
CFLAGS	= Data=FarOnly IdentifierLength=40 IncludeDirectory=library Optimize OptimizerSchedule NoStackCheck NoVersion CPU=$(CPU)
CFSPEED	= OptimizerTime OptimizerComplexity=10 OptimizerInLocal
CFLAGC	=
CFLAGD	= Define=
CFLAGO	= ObjectName=
CFLAGP	= ProgramName=
LDFLAGS = Link Data=FarOnly
MKLIB	= $(VAMOS) slink Quiet Lib lib:sc.lib To $@ From $^
CCVER	= $(VAMOS) sc | awk '/^SAS/ { printf " "$$0 }'
endif

ifeq ($(CCVER),)
$(error supported TARGETs are AmigaG, AmigaS, AmigaV)
endif
endif

PROG	= test
TESTNUMS = 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22
# OBJDIR is overridden by recursive sub-make invocations per CPU
OBJDIR	?= obj

OBJS	= $(addprefix $(OBJDIR)/,archivefs_api.o archivefs_common.o archivefs_lha.o archivefs_zip.o \
	archivefs_huffman_decoder.o archivefs_zip_decompressor.o)

ifeq (,$(findstring Amiga,$(TARGET)))
# non Amiga targets
OBJS += $(addprefix $(OBJDIR)/,archivefs_lha_decompressor.o)
else
OBJS += $(addprefix $(OBJDIR)/,archivefs_unlha.o archivefs_lha_decompressor_68k.o)
endif
OBJS_TEST = $(addprefix $(OBJDIR)/,test.o CRC32.o $(INTEGRATION_OBJ)) $(OBJS)
OBJS_LIB = $(addprefix $(OBJDIR)/,archivefs_header.o archivefs_integration_amiga_standalone.o) $(OBJS)

# all target: varies by context
# - non-Amiga: test binary only
# - Amiga sub-make (_MKCPU set): one specific library
# - Amiga top-level + profiling: test binary only (library build broken with -pg)
# - Amiga top-level normal: test binary + both CPU variants of the library
ifeq (,$(findstring Amiga,$(TARGET)))
all: $(PROG)
else ifdef _MKCPU
all: $(LIB)
else ifdef _MKTEST
all: test.020
else ifdef _PROF_NOLIB
all: $(PROG)
else
all: $(PROG) test.020 WHDLoad.VFS WHDLoad.VFS.020
endif

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: %.S .date .ccver | $(OBJDIR)
	$(AS) -o $@ $<

$(OBJDIR)/archivefs_integration_amiga_standalone.o: archivefs_integration_amiga.c .target | $(OBJDIR)
	$(CC) $(CFLAGS) $(CFLAGO)$@ $(CFLAGC) $< $(CFLAGD)ARCHIVEFS_STANDALONE=1

$(OBJDIR)/archivefs_huffman_decoder.o: CFLAGS+=$(CFSPEED)
$(OBJDIR)/archivefs_lha_decompressor.o: CFLAGS+=$(CFSPEED)
$(OBJDIR)/archivefs_zip_decompressor.o: CFLAGS+=$(CFSPEED)
$(OBJDIR)/%.o: %.c .target | $(OBJDIR)
	$(CC) $(CFLAGS) $(CFLAGO)$@ $(CFLAGC) $<

$(PROG): $(OBJS_TEST)
	$(CC) $(LDFLAGS) $(CFLAGP)$@ $^

# In sub-make: build the library passed via LIB=
# At top-level: dispatch two sub-makes, one per CPU, into separate obj dirs
ifdef _MKCPU
$(LIB): $(OBJS_LIB)
	$(MKLIB)
else ifdef _MKTEST
test.020: $(OBJS_TEST)
	$(CC) $(LDFLAGS) $(CFLAGP)$@ $^
else ifneq (,$(findstring Amiga,$(TARGET)))
WHDLoad.VFS WHDLoad.VFS.020:
	+$(MAKE) TARGET=$(TARGET) CPU=$(if $(filter %.020,$@),68020,68000) OBJDIR=$(if $(filter %.020,$@),obj.020,obj) LIB=$@ _MKCPU=1
test.020:
	+$(MAKE) TARGET=$(TARGET) CPU=68020 OBJDIR=obj.020 _MKTEST=1
endif

.date: FORCE
	@date "+(%d.%m.%Y)" | xargs printf > .date.tmp
	@cmp -s .date.tmp $@ || mv .date.tmp $@
	@rm -f .date.tmp

.ccver: FORCE
	@$(CCVER) > .ccver.tmp
	@cmp -s .ccver.tmp $@ || mv .ccver.tmp $@
	@rm -f .ccver.tmp

.target: FORCE
	@echo 'TARGET=$(TARGET)' > .target.tmp
	@cmp -s .target.tmp $@ || mv .target.tmp $@
	@rm -f .target.tmp

FORCE:

ifdef _MKCPU
.PHONY: all clean FORCE
else ifdef _MKTEST
.PHONY: all clean FORCE
else
.PHONY: all clean FORCE WHDLoad.VFS WHDLoad.VFS.020 test.020
endif

clean:
	rm -f $(PROG) test.020 WHDLoad.VFS WHDLoad.VFS.020 *~ */*~ .date .ccver .target *.lnk
	rm -rf obj obj.020

ifeq (,$(findstring Amiga,$(TARGET)))
run_tests: $(PROG)
	@for n in $(TESTNUMS); do ./testing/run_test.sh testing/test$$n.txt || exit 1; done
else
run_tests: $(PROG) test.020
	@for n in $(TESTNUMS); do ./testing/run_test.sh testing/test$$n.txt || exit 1; done
	@for n in $(TESTNUMS); do ./testing/run_test.sh testing/test$$n.txt ./test.020 || exit 1; done
endif

