#!/bin/bash
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Make edits to *.java and *.pgcfg
# Then run: ./build.sh | less

set -e
cd $(dirname $0)

# Returns the first parameter after ensuring the path exists.
function get_path() {
  if [[ ! -f $1 ]]; then
    >&2 echo "Pattern matched no files: $1"
    exit 1
  fi
  echo "$1"
}

ANDROID_JAR=$(get_path ../../android_sdk/public/platforms/*/android.jar)
DEXDUMP=$(get_path ../../android_sdk/public/build-tools/*/dexdump)
R8_PATH=$(get_path ../cipd/lib/r8.jar)
JAVA_HOME=../../jdk/current
JAVA_BIN=../../jdk/current/bin
MIN_API=24

# E.g.:
# EXTRA_JARS=../../../out/Release/obj/components/autofill_assistant/browser/proto_java.javac.jar:../../../out/Release/obj/clank/third_party/google3/protobuf.processed.jar

# Uncomment to create r8inputs.zip:
# DUMP_INPUTS=-Dcom.android.tools.r8.dumpinputtofile=r8inputs.zip

rm -f *.class
$JAVA_BIN/javac -cp $ANDROID_JAR:$EXTRA_JARS --release 11 *.java
$JAVA_BIN/java -cp $R8_PATH $DUMP_INPUTS com.android.tools.r8.R8 \
    --min-api $MIN_API \
    --lib "$JAVA_HOME" \
    --lib "$ANDROID_JAR" \
    --no-minification \
    --pg-conf playground.pgcfg \
    --pg-map-output Playground.mapping \
    --output . \
    ${EXTRA_JARS/:/ } \
    *.class
$DEXDUMP -d classes.dex > dexdump.txt

du -b *.dex
echo 'Outputs are: Playground.mapping, classes.dex, dexdump.txt'

