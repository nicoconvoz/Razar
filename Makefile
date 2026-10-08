# CC is assigned with := because make defines a built-in CC=cc, which makes
# ?= a no-op. A command-line `make CC=clang` still overrides this.
CC      := gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDLIBS  ?= -lm

SRC     := src/razar.c src/lego.c
HDR     := src/razar.h src/lego.h
BIN     := razar
TESTBIN := run_tests

.PHONY: all test clean

all: $(BIN)

$(BIN): src/main.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -Isrc -o $@ src/main.c $(SRC) $(LDLIBS)

$(TESTBIN): tests/test_razar.c tests/harness.h $(SRC) $(HDR)
	$(CC) $(CFLAGS) -Isrc -Itests -o $@ tests/test_razar.c $(SRC) $(LDLIBS)

test: $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -f $(BIN) $(TESTBIN) $(BIN).exe $(TESTBIN).exe
