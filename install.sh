#!/bin/bash
#
# Minimal install of N7NIX version of dantracker

package_name="dantracker"
FILELIST="aprs aprs-ax25"
JQUERY_DIR="/usr/share/$package_name/jQuery"

#
# Function checkDir
#  arg: directory to verify
#  -- creates directory $1, if it does not exist
checkDir() {
    if [ ! -d "$1" ] ; then
        mkdir -p "$1"
    fi
}

#
# Main
#

# Verify that binary executables exist
for binfile in `echo ${FILELIST}` ; do
if [ ! -e "$binfile" ] ; then
    echo "file $binfile DOES NOT exist, run make"
    exit 1
fi
done

#
# Be root
if [[ $EUID -ne 0 ]]; then
    echo "*** You must be root user ***" 2>&1
    exit 1
fi

# Test if directory /usr/share/dantracker exists
checkDir /usr/share/$package_name
# Test if directory /usr/share/dantracker/jQuery exists
checkDir $JQUERY_DIR

# Test if jquery javascript library has already been installed
if [ ! -e "$JQUERY_DIR/jquery.js" ] ; then
    echo "Installing jQuery directory"
    wget http://bit.ly/jqsource -O jquery.js
    jqversion=$(grep -m 1 -i core_version jquery.js | cut -d '"' -f2 | cut -d '"' -f1)
    echo "installing jquery version $jqversion"
    cp jquery.js $JQUERY_DIR
else
    echo "jQuery libraries ALREADY installed"
fi

# Test if directory /etc/tracker exists
checkDir /etc/tracker

# Test if config files already exist
if [ ! -e "/etc/tracker/aprs_tracker.ini" ] ; then
    echo "Copying config files"
    rsync -av examples/aprs_spy.ini /etc/tracker
    cp examples/aprs_tracker.ini /etc/tracker
fi

# Copy essential files
cp scripts/tracker* /etc/tracker
cp scripts/.screenrc.trk /etc/tracker
cp scripts/.screenrc.spy /etc/tracker
rsync -av --cvs-exclude --include "*.js" --include "*.html" --include "*.css" --exclude "*" webapp/ /usr/share/$package_name/
rsync -av --cvs-exclude --include "*.png" --exclude "Makefile" images /usr/share/$package_name/

echo "Install tracker & spy binaries"
cp aprs /usr/local/bin
cp aprs-ax25 /usr/local/bin

echo "Install finished"
exit 0
