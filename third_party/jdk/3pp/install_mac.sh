#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

# The directory structure will be like "jdk-23.0.2+7/Contents/{Home,MacOS,...}".
# We rename the root dir "jdk-23.0.2+7" to "current"
mv jdk-* current

mv current/Contents/Home/bin/java current/Contents/Home/bin/java.chromium
echo '#!/bin/sh

# https://crbug.com/1441023
exec "$0.chromium" -XX:+PerfDisableSharedMem "$@"
' > current/Contents/Home/bin/java
chmod a+x current/Contents/Home/bin/java

mv current/Contents "$PREFIX"
