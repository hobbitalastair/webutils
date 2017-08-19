LIBS = -lcurl -lhubbub `pkg-config --libs libnsfb`
CC = gcc
CFLAGS = -Wall -Werror -O2 -g
OBJS = webtoon-scrape webtoon-viewer tapas-scrape

all: $(OBJS)

%: %.c
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

clean:
	rm -f $(OBJS)
