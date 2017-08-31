LIBS = -lcurl -lhubbub `pkg-config --libs libnsfb`
CC = gcc
CFLAGS = -Wall -Werror -O2 -g
OBJS = comic-viewer webtoon-scrape tapas-scrape

all: $(OBJS)

%: %.c
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

clean:
	rm -f $(OBJS)
