CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -Wno-unused-parameter -D_DEFAULT_SOURCE -g -O0
LDFLAGS = -lX11 -lcrypto -lpthread

SRCDIR = $(shell basename $(shell pwd))
DESTDIR ?= 
PREFIX ?= /usr

SRC0 =  src/main.c src/lscreen.c src/util.c src/status.c src/rundlg.c
OBJ0 = $(SRC0:%.c=%.c.o)
EXE0 = swm

all: $(EXE0)
	
$(EXE0): $(OBJ0)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/config.h $(OBJ0) $(EXE0)

install:
	cp $(EXE0) $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE0)

dist:
	cd .. && tar cvzf $(SRCDIR).tgz ./$(SRCDIR)

