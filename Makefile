SHELL = /bin/sh

UNAME = $(shell uname | tr '[:upper:]' '[:lower:]')

ifeq ($(UNAME),darwin)
# Mac OS X
CC ?= clang
DLLFLAGS ?= -bundle -undefined dynamic_lookup
else
# assume Linux otherwise
CC ?= gcc
DLLFLAGS ?= -shared
endif

RM = rm -f
# RMDIR = rm -rf

LUADIR = /usr/local/include
SQLITEDIR = /usr/local/Cellar/sqlite/3.7.17

# COPT ?= -O2
COPT = -g
CWARN ?= -Wall -Wextra
# CVERSION ?= -ansi -pedantic
CFLAGS ?= $(COPT) $(CWARN) $(CVERSION) -fPIC -I$(LUADIR) -I$(SQLITEDIR)/include

LIBS = -L$(SQLITEDIR)/lib -lsqlite3

N = dblite
C = $N.c
O = $N.o
T = $N.so

all: $T

clean:
	$(RM) $O $T

echo:
	@echo CC = $(CC)
	@echo CFLAGS = $(CFLAGS)
	@echo DLLFLAGS = $(DLLFLAGS)
	@echo RM = $(RM)
	@echo T = $T
	@echo C = $C

$T: $O
	$(CC) $(DLLFLAGS) -o $@ $^ $(LIBS)

$O: $C $H
	$(CC) $(CFLAGS) -c $<
