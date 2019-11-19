#!/bin/bash
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented libnih1.

# Use the system-installed nih-dbus-tool during building. Normally a
# just-built one is used, however in MSan builds it will crash due to
# uninstrumented dependencies.

sed -i 's|NIH_DBUS_TOOL="\\${top_builddir}/nih-dbus-tool/nih-dbus-tool"|NIH_DBUS_TOOL="/usr/bin/nih-dbus-tool"|g' configure
