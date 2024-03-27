#!/bin/bash
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented libgdk-pixbuf2.0-0.

# Use the system-installed gdk-pixbuf-query-loaders during building. Normally a
# just-built one is used, however in MSan builds it will crash due to
# uninstrumented dependencies.

sed -i "s|gdk_pixbuf_query_loaders,|'/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders',|g" \
	gdk-pixbuf/meson.build

# gdk-pixbuf-print-mime-types requires instrumented libgio, which is unavailabe
# during the build.  It's also not installed in /usr/bin, so we must build
# an uninstrumented version ourselves and patch the build file to use that.
gcc thumbnailer/gdk-pixbuf-print-mime-types.c -o gdk-pixbuf-print-mime-types \
	$(pkg-config gdk-pixbuf-2.0 --libs --cflags)
sed -i "s|gdk_pixbuf_print_mime_types.full_path()|'../gdk-pixbuf-print-mime-types'|g" \
	thumbnailer/meson.build
