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
# functions to wrap when running tests
TEST_MOCKS := -Wl,-wrap,strndup -Wl,-wrap,malloc

# path to all source files, excluding extension. allows one level of nesting in src/*/*.c
SRCNAMES = ${subst $(SRCDIR)/,,$(basename $(wildcard $(SRCDIR)/*.c))\
							   $(basename $(wildcard $(SRCDIR)/*/*.c))}
# path to all source files, with extension
SRCS = $(patsubst %,$(SRCDIR)/%.c,$(SRCNAMES))
# path to all object files, with extension
OBJS = $(patsubst %,$(LIBDIR)/%.o,$(SRCNAMES))

# compile binary
all: $(OBJS)
	$(CC) -o $(BINDIR)/$(BINARY) $+ -O3 $(STD)

# rule for generating o files, allows one level of nesting in lib/*/*.o
$(LIBDIR)/%.o: $(SRCDIR)/%.c
	@mkdir --parents $(dir $@);
	$(CC) -c $^ -o $@

# rule for running tests
tests: $(filter-out lib/main.o, $(OBJS))
	$(CC) $(TESTDIR)/main.c $+ -I $(SRCDIR) -o $(BINDIR)/$(TEST_BINARY) $(TEST_LIBS) $(TEST_MOCKS)
	./$(BINDIR)/$(TEST_BINARY)

# rule for cleaning the project
clean:
	@rm -rvf $(BINDIR)/* $(LIBDIR)/* $(LOGDIR)/*;