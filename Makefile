SHELL:=/bin/bash

# Set DEBUG_ON to YES or NO
#  DEBUG_ON=YES enables pr_debug macro
DEBUG_ON=YES
WEBAPP_ENABLE=YES
GTKAPP_ENABLE=NO

DEST="root@beagle:carputer"

# The icon size is used by:
#  - convert in the images Makefile,
#  - ui.c as APRS_IMG_MULT
#  - tracker-frontend.js as APRS_IMG_MULT
APRS_IMG_MULT=4
APRS_ICON_SIZE=${APRS_IMG_MULT}00%
# Make available to Makefile in images directory
export APRS_ICON_SIZE

CFLAGS = -g -Wall -Werror
LIBS = -lfap -liniparser

TARGETS = aprs fakegps aprs-is faptest

ifeq ($(GTKAPP_ENABLE), YES)
GTK_CFLAGS = `pkg-config --cflags 'gtk+-2.0'`
GTK_LIBS = `pkg-config --libs 'gtk+-2.0'`
TARGETS += ui uiclient
endif

LIBAX25=$(shell ./conftest.sh)
BUILD=$(shell cat .build)
REVISION=$(shell cat .revision)

CFLAGS+=-DBUILD="\"$(BUILD)\"" -DREVISION="\"$(REVISION)\""

ifeq ($(LIBAX25), -lax25)
TARGETS+= aprs-ax25
CFLAGS+= -DHAVE_AX25_TRUE
LIBS+=$(LIBAX25)
endif

# -g option compiles with symbol table
ifeq ($(DEBUG_ON), YES)
CFLAGS+=-DDEBUG -g
endif

ifeq ($(WEBAPP_ENABLE), YES)
CFLAGS+=-DWEBAPP
LIBS+=-ljson-c
endif

.PHONY :  images clean sync install

all : $(TARGETS) images

aprs.o: aprs.c uiclient.c serial.c nmea.c aprs-is.c aprs-ax25.c aprs-msg.c conf.c util.o util.h aprs.h Makefile
uiclient.o: uiclient.c ui.h util.c util.h Makefile
aprs-is.o: aprs-is.c conf.c util.c aprs-is.h util.h aprs.h Makefile
aprs-ax25.o: aprs-ax25.c aprs-ax25.h aprs.h conf.c util.c aprs-is.h util.h Makefile ax25dump.c
serial.o: serial.c serial.h
nmea.o: nmea.c nmea.h
util.o: util.c util.h
conf.o: conf.c util.c util.h aprs.h
ax25dump.o: ax25dump.c crc.c util.c monixd.h util.h
crc.o: crc.c
faptest.o: faptest.c


aprs: aprs.o uiclient.o nmea.o aprs-is.o serial.o aprs-ax25.o aprs-msg.o conf.o util.o
#	@echo "libs: $(LIBS), cflags: $(CFLAGS), libax25: $(LIBAX25), build: $(BUILD), rev: $(REVISION)"
	test -d .hg && hg id --id > .revision || true
	echo $$((`cat .build` + 1)) > .build
	$(CC) $(CFLAGS) $(APRS_CFLAGS) -o $@ $^  $(LIBS)

ui: ui.c uiclient.o ui.h util.c util.h
	$(CC) $(CFLAGS) -DAPRS_IMG_MULT=${APRS_IMG_MULT} $(GTK_CFLAGS) $(GLIB_CFLAGS) $^ -o $@ $(GTK_LIBS) $(GLIB_LIBS) $(LIBS)

uiclient: uiclient.c ui.h util.c util.h
	$(CC) $(CFLAGS) -DMAIN $< -o $@ util.o -lm $(LIBS)

aprs-is: aprs-is.c conf.o util.o util.h aprs-is.h aprs.h
	$(CC) $(CFLAGS) -DMAIN  $< -o $@ conf.o util.o -lm $(LIBS)

aprs-ax25: aprs-ax25.c conf.o util.o util.h aprs-is.h aprs-ax25.h aprs.h ax25dump.o crc.o
	$(CC) $(CFLAGS) -DMAIN  $< -o $@ conf.o util.o ax25dump.o crc.o -lm $(LIBS)

fakegps: fakegps.c
	$(CC) $(CFLAGS) -o $@ $< -lm

faptest: faptest.c
	$(CC) $(CFLAGS) -o $@ $< -lm $(LIBS)

images:
	$(MAKE) -C images

clean:
	rm -f $(TARGETS) *.o *~ ./images/*_big.png

sync:
	scp -r *.c *.h Makefile tools images .revision .build $(DEST)

# copy node.js files to /usr/share/dantracker/
# use rsync to create possible missing directories
install:
	rsync -a examples/aprs_spy.ini /etc/tracker
	cp examples/aprs_spy.ini /etc/tracker
	cp scripts/tracker* /etc/tracker
	cp scripts/.screenrc.trk /etc/tracker
	cp scripts/.screenrc.spy /etc/tracker
	rsync -a --cvs-exclude --include "*.js" --include "*.html" --exclude "*" webapp/ /usr/share/dantracker/
	cp aprs /usr/local/bin
	cp aprs-ax25 /usr/local/bin
