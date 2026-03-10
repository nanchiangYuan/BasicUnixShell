CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
CFLAGS-dbg = -Wall -Wextra -Werror -pedantic -std=gnu18 -g

SRCC = main.c commands.c execute.c parse.c
SRCH = bsh.h

TARGET = bsh

all: $(TARGET) $(TARGET)-dbg

$(TARGET): $(SRCC) $(SRCH)
	$(CC) $(CFLAGS) $(SRCC) -o $@

$(TARGET)-dbg: $(SRCC) $(SRCH)
	$(CC) $(CFLAGS-dbg) $(SRCC) -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg

.PHONY: all clean
