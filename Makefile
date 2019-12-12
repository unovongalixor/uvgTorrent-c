# project name
PROJECT_NAME := uvgTorrent-c

# binary output
BINARY := uvgTorrent

# compiling
CC := gcc
STD := -std=gnu99

# paths
BINDIR := bin
SRCDIR := src
LOGDIR := log
LIBDIR := lib
TESTDIR := test

# source and objects
SRCNAMES = ${subst $(SRCDIR)/,,$(basename $(wildcard $(SRCDIR)/*.c))\
							   $(basename $(wildcard $(SRCDIR)/*/*.c))}

SRCS = $(patsubst %,$(SRCDIR)/%.c,$(SRCNAMES))
OBJS = $(patsubst %,$(LIBDIR)/%.o,$(SRCNAMES))

# rule for generating
$(OBJS): $(SRCS)
	$(CC) -c $^ -o $@

all: $(OBJS)
	$(CC) -o $(BINDIR)/$(BINARY) $+ -O3 $(STD)