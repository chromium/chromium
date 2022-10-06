#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented libidn.

# Do not warn about undefined sanitizer symbols in object files.
sed -i "s/-no-undefined//g" ./lib/gl/Makefile.am
sed -i "s/-no-undefined//g" ./lib/Makefile.am
sed -i "s/-Wl,-z,defs//g" ./debian/rules

# Do not run tests.
sed -i "s/$(MAKE) check//g" ./debian/rules
