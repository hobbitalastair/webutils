PREFIX := ${DESTDIR}/usr
BINDIR := ${PREFIX}/bin
LIBS = -lcurl -lhubbub `pkg-config --libs libnsfb`
CC = gcc
CFLAGS = -Wall -Werror -O2 -g
BIN = comic-viewer html-extract links2atom scrape-webtoon scrape-tapas

all: $(BIN)

%: %.c
	$(CC) -o $@ $< $(LIBS) $(CFLAGS)

install: $(BIN)
	mkdir -p "${BINDIR}/"
	for bin in $(BIN); do \
	    install -m755 "$$bin" "${BINDIR}/"; \
	done

clean:
	rm -f $(OBJS)
