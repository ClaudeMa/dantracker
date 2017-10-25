#!/bin/bash
#
# Build dependencies required for dantracker
# Does NOT build dantracker
# Use to setup environment for building dantracker
#
# Builds & copies these files
#
# Builds:
#   - libiniparser
#   - libfap
#   - json-c
#
# How to install latest version of node
# https://nodejs.org/en/download
#
DEBUG=1
FORCE_BUILD="false"

scriptname="`basename $0`"
user=$(whoami)

SRC_DIR="/home/$user/dev/github/dep_src"

TRACKER_SRC_DIR="/home/$user/dev/github/dantracker"
LIBFAP_SRC_DIR="$SRC_DIR/libfap"
JSON_C_SRC_DIR="$SRC_DIR/json-c"
LIBFAP_VER="1.5"
LIBINIPARSER_VER="3.1"

PKGLIST="build-essential pkg-config imagemagick automake autoconf libtool libgps-dev iptables screen"

# ==== main

# as root install a bunch of stuff
sudo apt-get -y install $PKGLIST


if [ -d $SRC_DIR/libfap-$LIBFAP_VER ] ; then
   echo "** already have libfap-$LIBFAP_VER source"
else
   echo
   echo "== get libfap source"
   # Get libfap [http://pakettiradio.net/libfap/]
   # To get latest version, index of downloads is here:
   # http://www.pakettiradio.net/downloads/libfap/

   cd $LIBFAP_SRC_DIR
   wget http://pakettiradio.net/downloads/libfap/$LIBFAP_VER/libfap-$LIBFAP_VER.tar.gz
   tar -zxvf libfap-$LIBFAP_VER.tar.gz

   echo
   echo "== build libfap"
   # run the fap patch
   cd libfap-$LIBFAP_VER
   patch -p2 < $TRACKER_SRC_DIR/fap_patch.n7nix
   sudo cp src/fap.h /usr/local/include/
   ./configure
   make
   sudo make install
fi

if [ -d $SRC_DIR/iniparser ] ; then
   echo "** already have iniparser source"
else
   echo
   echo "== get libiniparser source"
   cd $SRC_DIR
   wget http://ndevilla.free.fr/iniparser/iniparser-$LIBINIPARSER_VER.tar.gz
   tar -zxvf iniparser-$LIBINIPARSER_VER.tar.gz
   echo
   echo "== build libiniparser source"
   cd iniparser
   sudo cp src/iniparser.h  /usr/local/include
   sudo cp src/dictionary.h /usr/local/include
   make
   sudo cp libiniparser.* /usr/local/lib
fi

if [ -d $JSON_C_SRC_DIR ] && [ "$FORCE_BUILD" = "false" ]; then
   echo "** already have json-c source"
else

   echo
   echo "== get json-c"
   cd $SRC_DIR

   #  https://github.com/json-c/json-c
   git clone git://github.com/json-c/json-c.git

   echo "== build json-c"
   cd json-c

   sh autogen.sh
   ./configure
   make
   sudo make install
   sudo ldconfig
   if [ -f /usr/local/include/json-c/json.h ] ; then
      sudo mkdir /usr/local/include/json
      sudo cp /usr/local/include/json-c/*.h /usr/local/include/json
   fi
fi

