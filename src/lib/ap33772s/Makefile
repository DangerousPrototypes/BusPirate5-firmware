CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11
CFLAGS += -I.
LDFLAGS ?=

SRCS = main.c ap33772s.c
OBJS = main.o ap33772s.o
TARGET = ap33772s_demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c ap33772s.h ap33772s_int.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
