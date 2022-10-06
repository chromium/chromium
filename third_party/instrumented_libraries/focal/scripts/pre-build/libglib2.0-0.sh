#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use the system-installed glib-compile-resources during building. Normally a
# just-built one is used, however in MSan builds it will crash due to
# uninstrumented dependencies.

sed -i "s|glib_compile_resources|'/usr/bin/glib-compile-resources'|g" \
	gio/tests/meson.build
