# SLR Parser Generator - Team 7
#   make          build ./slr
#   make test     build and run the test suite
#   make demo     run the Review 2 demo commands
#   make clean

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS += -Iinclude

LIB_SRC = src/grammar.c src/lexer.c src/lr0.c src/first_follow.c \
          src/slr_table.c src/sdt.c src/parser.c src/pipeline.c
HEADERS = $(wildcard include/*.h)

all: slr

slr: $(LIB_SRC) src/main.c $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIB_SRC) src/main.c -o $@

test_slr: $(LIB_SRC) tests/test_slr.c $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIB_SRC) tests/test_slr.c -o $@

test: test_slr
	./test_slr

demo: slr
	./slr --all "x = a + b * c"
	-./slr --file examples/program.txt
	-./slr -g grammars/ambiguous.grammar
	./slr -g grammars/extended.grammar "r = a - b / c - d"

clean:
	rm -f slr test_slr slr.exe test_slr.exe

.PHONY: all test demo clean
