#!/bin/bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before the build of instrumented libcups2.

# Libcup2 configure script, if the compiler name ends with "clang", enables PIE
# with a -Wl,-pie flag. That does not work at all, because the driver running in
# non-PIE mode links incompatible crtbegin.o (or something similarly named).

sed -i "s|-Wl,-pie|-pie|g" configure
