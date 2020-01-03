# project name
PROJECT_NAME := uvgTorrent-c

# binary output
BINARY := uvgTorrent

# compiling
CC := gcc
STD := -std=gnu99
LIBS := -L /usr/lib -l curl -l pthread -l m

# paths
BINDIR := bin
SRCDIR := src
LOGDIR := log
LIBDIR := lib
TESTDIR := test

# test related
TEST_LIBS := -l cmocka
TEST_BINARY := $(BINARY)_test_runner
# functions to wrap when running tests
TEST_MOCKS := -Wl,-wrap,strndup -Wl,-wrap,malloc -Wl,-wrap,connect_wait -Wl,-wrap,read -Wl,-wrap,write -Wl,-wrap,random -Wl,-wrap,poll -Wl,-wrap,getaddrinfo -Wl,-wrap,socket

# path to all source files, excluding extension. allows one level of nesting in src/*/*.c
SRCNAMES = ${subst $(SRCDIR)/,,$(basename $(wildcard $(SRCDIR)/*.c))\
							   $(basename $(wildcard $(SRCDIR)/*/*.c))}
# path to all source files, with extension
SRCS = $(patsubst %,$(SRCDIR)/%.c,$(SRCNAMES))
# path to all object files, with extension
OBJS = $(patsubst %,$(LIBDIR)/%.o,$(SRCNAMES))

# compile binary
all: $(OBJS)
	$(CC) $(STD) -o $(BINDIR)/$(BINARY) $+ -O3 $(STD) $(LIBS)

# rule for generating o files, allows one level of nesting in lib/*/*.o
$(LIBDIR)/%.o: $(SRCDIR)/%.c
	@mkdir --parents $(dir $@);
	$(CC) $(STD) -c $^ -o $@ $(LIBS)

# rule for running tests
tests: $(filter-out src/main.c, $(SRCS))
	$(CC) --coverage $(STD) $(TESTDIR)/main.c $+ -I $(SRCDIR) -o $(BINDIR)/$(TEST_BINARY) $(LIBS) $(TEST_LIBS) $(TEST_MOCKS)
	./$(BINDIR)/$(TEST_BINARY)

# rule to run valgrind
valgrind:
	valgrind \
		--track-origins=yes \
		--leak-check=full \
		--show-leak-kinds=all \
		--leak-resolution=high \
		--log-file=$(LOGDIR)/$@.log \
		$(BINDIR)/$(BINARY) --magnet_uri="magnet:?xt=urn:btih:3a6b29a9225a2ffb6e98ccfa1315cc254968b672&dn=Rick+and+Morty+S03E01+720p+HDTV+HEVC+x265-iSm&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%2F%2Fzer0day.ch%3A1337&tr=udp%3A%2F%2Fopen.demonii.com%3A1337&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969&tr=udp%3A%2F%2Fexodus.desync.com%3A6969" --path="/tmp"
	@echo "\nChecking the log file: $(LOGDIR)/$@.log\n"
	@cat $(LOGDIR)/$@.log


# rule for cleaning the project
clean:
	@rm -rvf $(BINDIR)/* $(LIBDIR)/* $(LOGDIR)/*;
