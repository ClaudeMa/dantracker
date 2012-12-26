SHELL:=/bin/bash

# Set DEBUG_ON to YES or NO
#  DEBUG_ON=YES enables pr_debug macro
DEBUG_ON=NO

CFLAGS = -g -Wall
LIBS = -lfap -liniparser
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`

DEST="root@beagle:carputer"

TARGETS = aprs ui uiclient fakegps aprs-is aprs-ax25

LIBAX25=$(shell ./conftest.sh)
BUILD=$(shell cat .build)
REVISION=$(shell cat .revision)

CFLAGS+=-DBUILD="\"$(BUILD)\"" -DREVISION="\"$(REVISION)\""

ifeq ($(LIBAX25), -lax25)
CFLAGS+= -DHAVE_AX25_TRUE
LIBS+=$(LIBAX25)
endif

ifeq ($(DEBUG_ON), YES)
CFLAGS+=-DDEBUG
endif


all: $(TARGETS)

aprs.o: aprs.c uiclient.c serial.c nmea.c aprs-is.c aprs-ax25.c conf.c util.o util.h aprs.h Makefile
uiclient.o: uiclient.c ui.h
aprs-is.o: aprs-is.c conf.c util.c aprs-is.h util.h aprs.h Makefile
aprs-ax25.o: aprs-ax25.c aprs-ax25.h aprs.h conf.c util.c aprs-is.h util.h Makefile
serial.o: serial.c serial.h
nmea.o: nmea.c nmea.h
util.o: util.c util.h
conf.o: conf.c util.c util.h aprs.h


aprs: aprs.o uiclient.o nmea.o aprs-is.o serial.o aprs-ax25.o conf.o util.o
#	@echo "libs: $(LIBS), cflags: $(CFLAGS), libax25: $(LIBAX25), build: $(BUILD), rev: $(REVISION)"
	test -d .hg && hg id --id > .revision || true
	echo $$((`cat .build` + 1)) > .build
	$(CC) $(CFLAGS) $(APRS_CFLAGS) -o $@ $^  $(LIBS)

ui: ui.c uiclient.o
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $^ -o $@ $(GTK_LIBS) $(GLIB_LIBS)

uiclient: uiclient.c ui.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@

aprs-is: aprs-is.c conf.o util.o util.h aprs-is.h aprs.h
	$(CC) $(CFLAGS) -DMAIN  $< -o $@ conf.o util.o -lm $(LIBS)

aprs-ax25: aprs-ax25.c conf.o util.o util.h aprs-is.h aprs-ax25.h aprs.h
	$(CC) $(CFLAGS) -DMAIN  $< -o $@ conf.o util.o -lm $(LIBS)

fakegps: fakegps.c
	$(CC) $(CFLAGS) -lm -o $@ $< -lm

clean:
	rm -f $(TARGETS) *.o *~

sync:
	scp -r *.c *.h Makefile tools images .revision .build $(DEST)
