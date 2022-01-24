#!/bin/bash
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented xkbcommon.

# Do not warn about undefined sanitizer symbols in object files.
sed -i "s/\(-Wl,--no-undefined\|-Wl,-z,defs\)//g" ./Makefile.am

# Do not warn about uninstalled documentation.
sed -i "s/--fail-missing//g" ./debian/rules

# Do not warn about extra msan symbols.
sed -i "s/dh_makeshlibs -- -c4/dh_makeshlibs/g" ./debian/rules
