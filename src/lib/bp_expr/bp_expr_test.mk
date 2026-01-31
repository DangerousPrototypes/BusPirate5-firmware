# bp_expr test makefile
# Usage: make -f bp_expr_test.mk

CC ?= gcc
CFLAGS = -Wall -Wextra -I../.. -g

SRCS = bp_expr_test.c bp_expr.c ../bp_number/bp_number.c
TARGET = bp_expr_test

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
