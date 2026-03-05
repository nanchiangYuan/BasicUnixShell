CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
CFLAGS-dbg = -Wall -Wextra -Werror -pedantic -std=gnu18 -g
LOGIN = nanchiang
SUBMITPATH = ~cs537-1/handin/$(LOGIN)
ORIGINALPATH = /home/$(LOGIN)/private/cs537/
FILEPATH = /home/$(LOGIN)/private/cs537/p3/

TARGET = wsh

all: $(TARGET) $(TARGET)-dbg

$(TARGET):  $(TARGET).c  $(TARGET).h
	$(CC) $(CFLAGS) $< -o $@

$(TARGET)-dbg: $(TARGET).c $(TARGET).h
	$(CC) $(CFLAGS-dbg) $< -o $@

test: test.c
	$(CC) $(CFLAGS-dbg) $< -o $@

clean:
	rm -f $(TARGET) $(TARGET)-dbg
	rm -f test

submit: clean
	rm -f test.c
	cd $(ORIGINALPATH)
	cp -r $(FILEPATH) $(SUBMITPATH)

valgrind: $(TARGET) $(TARGET)-dbg
	valgrind --leak-check=full --show-leak-kinds=all $(TARGET)-dbg

