#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Make edits to *.java and *.pgcfg
# Then run: ./build.sh | less


# Finds any version of dexdump available
dexdumps=( ../../android_sdk/public/build-tools/*/dexdump )
DEXDUMP=${dexdumps[0]}

rm -f *.class
javac *.java && \
java -cp ../lib/r8.jar com.android.tools.r8.R8 *.class --output . --lib ../../jdk/current --no-minification --pg-conf playground.pgcfg && \
$DEXDUMP -d classes.dex > dexdump.txt
echo 'dexdump.txt updated.'
