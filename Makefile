CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pthread -g
TARGET  = mtws
SRCS    = mtws.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
