LIBS = -lcurl -lhubbub
CC = gcc
CFLAGS = -Wall -Werror -O2 -g
OBJS = webtoon

all: $(OBJS)

webtoon: webtoon.c
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

clean:
	rm -f $(OBJS)
