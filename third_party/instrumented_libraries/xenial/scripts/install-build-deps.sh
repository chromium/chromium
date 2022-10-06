#!/bin/bash -e

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install build dependencies of packages which we instrument.

# Enable source repositories in Goobuntu.
if hash goobuntu-config 2> /dev/null
then
  sudo goobuntu-config set include_deb_src true
fi

# TODO(eugenis): find a way to pull the list from the build config.
packages="\
atk1.0 \
brltty \
dee \
dpkg-dev \
freetype \
gnome-common \
gobject-introspection \
libappindicator3-1 \
libasound2 \
libatk-bridge2.0-0 \
libatspi2.0-0 \
libavahi-client3 \
libcairo2 \
libcap2 \
libcups2 \
libcurl3-gnutls \
libdbus-1-3 \
libdbus-glib-1-2 \
libdbusmenu \
libdbusmenu-glib4 \
libexpat1 \
libffi6 \
libfontconfig1 \
libgdk-pixbuf2.0-0 \
libglib2.0-0 \
libgnome-keyring0 \
libgpg-error0 \
libgraphite2-dev \
libgtk-3-0 \
libgtk2.0-bin \
libidn11 \
libido3-0.1-0 \
libindicator3-7 \
libjasper1 \
libjpeg-turbo8 \
libmicrohttpd-dev \
libnspr4 \
libp11-kit0 \
libpci3 \
libpcre3 \
libpixman-1-0 \
libpng12-0 \
librtmp-dev \
libsasl2-2 \
libunity9 \
libwayland-client0 \
libx11-6 \
libxau6 \
libxcb1 \
libxcomposite1 \
libxcursor1 \
libxdamage1 \
libxdmcp6 \
libxext6 \
libxfixes3 \
libxi6 \
libxinerama1 \
libxkbcommon0 \
libxrandr2 \
libxrender1 \
libxss1 \
libxtst6 \
nss \
pango1.0 \
pkg-config \
pulseaudio \
udev \
zlib1g"

# Extra build deps for pulseaudio, which apt-get build-dep may fail to install
# for reasons which are not entirely clear.
sudo apt-get install libltdl3-dev libjson0-dev \
         libsndfile1-dev libspeexdsp-dev libjack0 \
         chrpath -y  # Chrpath is required by fix_rpaths.sh.

# Needed for libldap-2.4.2. libldap is not included in the above list because
# one if its dependencies, libgssapi3-heimdal, conflicts with libgssapi-krb5-2,
# required by libcurl. libgssapi3-heimdal isn't required for this build of
# libldap.
sudo apt-get install libsasl2-dev -y

sudo apt-get build-dep -y $packages

# Work around an issue where clang builds search for libapparmor.so in the wrong
# path.  This is required for building udev, pulseaudio, and libdbus-1-3.
sudo ln -s /usr/lib/x86_64-linux-gnu/libapparmor.so \
    /lib/x86_64-linux-gnu/libapparmor.so