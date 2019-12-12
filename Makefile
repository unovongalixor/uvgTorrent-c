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

# test related
TEST_LIBS := -l cmocka -L /usr/lib
TEST_BINARY := $(BINARY)_test_runner
TEST_MOCKS := -Wl,-wrap,strndup -Wl,-wrap,malloc


# source and objects. allows one level of nesting
SRCNAMES = ${subst $(SRCDIR)/,,$(basename $(wildcard $(SRCDIR)/*.c))\
							   $(basename $(wildcard $(SRCDIR)/*/*.c))}

SRCS = $(patsubst %,$(SRCDIR)/%.c,$(SRCNAMES))
OBJS = $(patsubst %,$(LIBDIR)/%.o,$(SRCNAMES))
OBJDIRS:=$(dir $(OBJS))

all: $(OBJS)
	$(CC) -o $(BINDIR)/$(BINARY) $+ -O3 $(STD)

# rule for generating
$(LIBDIR)/%.o: $(SRCDIR)/%.c
	mkdir --parents $(dir $@)
	$(CC) -c $^ -o $@

$(LIBDIR)/*/%.o: $(SRCDIR)/*/%.c
	$(CC) -c $^ -o $@

# rule for running tests
tests: $(filter-out lib/main.o, $(OBJS))
	$(CC) $(TESTDIR)/main.c $+ -I $(SRCDIR) -o $(BINDIR)/$(TEST_BINARY) $(TEST_LIBS) $(TEST_MOCKS)
	./$(BINDIR)/$(TEST_BINARY)


# Rule for cleaning the project
clean:
	@rm -rvf $(BINDIR)/* $(LIBDIR)/* $(LOGDIR)/*;