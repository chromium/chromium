#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

mv current/bin/java current/bin/java.chromium
echo '#!/bin/sh

# https://crbug.com/1441023
exec "$0.chromium" -XX:+PerfDisableSharedMem "$@"
' > current/bin/java
chmod a+x current/bin/java

mv current/* "$PREFIX"
