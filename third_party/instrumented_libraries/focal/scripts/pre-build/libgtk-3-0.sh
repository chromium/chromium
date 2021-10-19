#!/bin/bash
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented libgtk-3-0.

# Use the system-installed gtk-update-icon-cache during building. Normally a
# just-built one is used, however in MSan builds it will crash due to
# uninstrumented dependencies.

sed -i "s|./gtk-update-icon-cache|/usr/bin/gtk-update-icon-cache|g" gtk/Makefile.am

# Don't build immodules.cache.  It requires running just-built executables that
# depend on glib, but using the system glib will cause msan errors.  This file
# is only used in GTK test suites, and is unneeded for the instrumented build.

sed -i "s|all-local: immodules.cache||g" modules/input/Makefile.am

autoreconf
