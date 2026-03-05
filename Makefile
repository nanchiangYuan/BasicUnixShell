CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
CFLAGS-dbg = -Wall -Wextra -Werror -pedantic -std=gnu18 -g

SRCC = main.c commands.c execute.c parse.c

SRCH = bsh.h

TARGET = bsh

all: $(TARGET) $(TARGET)-dbg

$(TARGET): $(SRCC) $(SRCC)
	$(CC) $(CFLAGS) $< -o $@

$(TARGET)-dbg: $(SRCC) $(SRCC)
	$(CC) $(CFLAGS-dbg) $< -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg
