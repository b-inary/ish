
CC = gcc
CFLAGS = -Wall -O2

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

TARGET = ish

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJS): ish.h

clean:
	rm -f *~ $(TARGET) $(OBJS)

.PHONY: clean

