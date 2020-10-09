# Copyright (C) Teemu Suutari

CC	= clang
CFLAGS	= -g -Wall -Werror
LDFLAGS =

PROG	= test

OBJS	= container_api.o container_common.o container_integration.o container_lha.o container_zip.o test.o

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

clean:
	rm -f $(OBJS) $(PROG) *~
