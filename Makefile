# project name
PROJECT_NAME := uvgTorrent-c

# binary output
BINARY := uvgTorrent

# compiling
CC := gcc -g
STD := -std=gnu11
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
	valgrind \
    		--track-origins=yes \
    		--leak-check=full \
    		--show-leak-kinds=all \
    		--leak-resolution=high \
    		--log-file=$(LOGDIR)/test.log \
    		$(BINDIR)/$(TEST_BINARY)
	mv *.gcno coverage/gcov
	mv *.gcda coverage/gcov
	lcov --capture --directory coverage/gcov --output-file coverage/coverage.info
	genhtml coverage/coverage.info --output-directory coverage/html
	@cat $(LOGDIR)/test.log

# rule to run valgrind
valgrind:
	valgrind \
		--track-origins=yes \
		--leak-check=full \
		--show-leak-kinds=all \
		--leak-resolution=high \
		--log-file=$(LOGDIR)/$@.log \
		$(BINDIR)/$(BINARY) --magnet_uri="magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Sintel&tr=udp%3A%2F%2Fexplodie.org%3A6969&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969&tr=udp%3A%2F%2Ftracker.empire-js.us%3A1337&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337" --path="/tmp"
	@echo "\nChecking the log file: $(LOGDIR)/$@.log\n"
	@cat $(LOGDIR)/$@.log

gdb:
	gdb -ex=run --args ./bin/uvgTorrent --magnet_uri="magnet:?xt=urn:btih:08ada5a7a6183aae1e09d831df6748d566095a10&dn=Sintel&tr=udp%3A%2F%2Fexplodie.org%3A6969&tr=udp%3A%2F%2Ftracker.coppersurfer.tk%3A6969&tr=udp%3A%2F%2Ftracker.empire-js.us%3A1337&tr=udp%3A%2F%2Ftracker.leechers-paradise.org%3A6969&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337" --path="/tmp"


# rule for cleaning the project
clean:
	@rm -rvf $(BINDIR)/* $(LIBDIR)/* $(LOGDIR)/*;
