CFLAGS=-W -Wall
PREFIX=/usr
BINDIR=$(PREFIX)/sbin
GROUP=wifisetup

all: wifisetup

clean:
	rm -f wifisetup

install:
	install -D -m 755 wifisetup $(BINDIR)/wifisetup

install-suid: install
	groupadd -f $(GROUP)
	chgrp $(GROUP) $(BINDIR)/wifisetup 
	chmod 4750 $(BINDIR)/wifisetup

wifisetup: wifisetup.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: all clean install install-suid
