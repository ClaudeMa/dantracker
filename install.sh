#!/bin/bash
#
package_name="dantracker"
FILELIST="aprs aprs-ax25"

#-- creates directory $1, if it does not exist
checkDir() {
    if [ ! -d "$1" ] ; then
        mkdir -p "$1"
    fi
}

for binfile in `echo ${FILELIST}` ; do
if [ ! -e "$binfile" ] ; then
  echo "file $binfile DOES NOT exist, run make"
  exit 1
fi

done

if [[ $EUID -ne 0 ]]; then
  echo "*** You must be root user ***" 2>&1
  exit 1
fi

# Test if directory /etc/tracker exists
checkDir /etc/tracker

# Test if config files already exist
if [ ! -e "/etc/tracker/aprs_tracker.ini" ] ; then
        echo "Copying config files"
        rsync -av examples/aprs_spy.ini /etc/tracker
        cp examples/aprs_tracker.ini /etc/tracker
fi

cp scripts/tracker* /etc/tracker
cp scripts/.screenrc.trk /etc/tracker
cp scripts/.screenrc.spy /etc/tracker
rsync -av --cvs-exclude --include "*.js" --include "*.html" --exclude "*" webapp/ /usr/share/$package_name/
rsync -av --cvs-exclude --include "*.png" --exclude "Makefile" images /usr/share/$package_name/
cp aprs /usr/local/bin
cp aprs-ax25 /usr/local/bin

exit 0