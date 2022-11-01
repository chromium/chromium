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
alsa-lib \
atk1.0 \
at-spi2-atk \
at-spi2-core \
avahi \
brltty \
cairo \
cups \
curl \
cyrus-sasl2 \
dbus \
dbus-glib \
dee \
dpkg \
expat \
fontconfig \
freetype \
gdk-pixbuf \
glib2.0 \
gnome-common \
gnome-keyring \
gobject-introspection \
graphite2 \
gtk+3.0 \
ido \
jasper-initramfs \
libappindicator3-1 \
libcap2 \
libdbusmenu \
libdbusmenu-gtk3-dev \
libffi \
libgpg-error \
libidn \
libindicator \
libjpeg-turbo \
libldap-2.4-2 \
libmicrohttpd \
libpng1.6 \
libsasl2-2 \
libssl1.1 \
libunity \
libx11 \
libxau \
libxcb \
libxcomposite \
libxcursor \
libxdamage \
libxdmcp \
libxext \
libxfixes \
libxi \
libxinerama \
libxkbcommon \
libxrandr \
libxrender \
libxss \
libxtst \
nspr \
nss \
opensc-pkcs11 \
p11-kit \
pango1.0 \
pciutils \
pcre3 \
pixman \
pkg-config \
pulseaudio \
rtmpdump \
systemd \
wayland \
zlib"

sudo apt-get build-dep -y $packages
