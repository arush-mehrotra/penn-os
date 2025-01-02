SRC_DIR = src
BIN_DIR = bin
LOG_DIR = log
DOC_DIR = doc
TESTS_DIR = tests

.PHONY: all tests info format clean

CC = gcc-12
CXX = g++-12
CFLAGS = -pthread -Wall -Werror -O0 -g --std=gnu2x
CXXFLAGS = -pthread -Wall -Werror -O0 -g --std=c++20

CPPFLAGS = -I $(SRC_DIR)

TEST_MAINS = $(TESTS_DIR)/sched-demo.c 

MAIN_FILES = $(SRC_DIR)/pennos.c $(SRC_DIR)/pennfat.c
EXECS = $(addprefix $(BIN_DIR)/, $(notdir $(MAIN_FILES:.c=)))
TEST_EXECS = $(subst $(TESTS_DIR),$(BIN_DIR),$(TEST_MAINS:.c=))

SRCS = $(filter-out $(MAIN_FILES), $(shell find $(SRC_DIR) -type f -name '*.c'))
HDRS = $(shell find src -type f -name '*.h')
OBJS = $(SRCS:.c=.o) src/util/parser.o

TEST_OBJS = $($(wildcard $(TESTS_DIR)/*.c):.c=.o)

CLEAN_OBJS = $(filter-out src/util/parser.o, $(OBJS))

all: $(EXECS)

tests: $(TEST_EXECS)

$(EXECS): $(BIN_DIR)/%: $(SRC_DIR)/%.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJS) $<

$(TEST_EXECS): $(BIN_DIR)/%: $(TESTS_DIR)/%.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJS) $(subst $(BIN_DIR)/,$(TESTS_DIR)/,$@).c

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $<

info:
	$(info MAIN_FILES: $(MAIN_FILES)) \
	$(info EXECS: $(EXECS)) \
	$(info SRCS: $(SRCS)) \
	$(info HDRS: $(HDRS)) \
	$(info OBJS: $(OBJS)) \
	$(info TEST_MAINS: $(TEST_MAINS)) \
	$(info TEST_EXECS: $(TEST_EXECS))

format:
	clang-format -i --verbose --style=Chromium $(MAIN_FILES) $(TEST_MAINS) $(SRCS) $(HDRS)

clean:
	rm -f $(CLEAN_OBJS) $(EXECS) $(TEST_EXECS)