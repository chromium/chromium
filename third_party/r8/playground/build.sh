#!/bin/bash
# Copyright 2020 The Chromium Authors. All rights reserved.
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
DESUGAR_JDK_LIBS_JSON=$(get_path ../desugar_jdk_libs.json)
DESUGAR_JDK_LIBS_JAR=$(get_path ../../android_deps/libs/com_android_tools_desugar_jdk_libs/desugar_jdk_libs-*.jar)
DESUGAR_JDK_LIBS_CONFIGURATION_JAR=$(get_path ../../android_deps/libs/com_android_tools_desugar_jdk_libs_configuration/desugar_jdk_libs_configuration-*.jar)
R8_PATH=$(get_path ../lib/r8.jar)
JAVA_HOME=../../jdk/current
JAVA_BIN=../../jdk/current/bin
MIN_API=21

# E.g.:
# EXTRA_JARS=../../../out/Release/obj/components/autofill_assistant/browser/proto_java.javac.jar:../../../out/Release/obj/clank/third_party/google3/protobuf.processed.jar

# Uncomment to create r8inputs.zip:
# DUMP_INPUTS=-Dcom.android.tools.r8.dumpinputtofile=r8inputs.zip

rm -f *.class
$JAVA_BIN/javac -cp $ANDROID_JAR:$EXTRA_JARS -target 11 -source 11 *.java
$JAVA_BIN/java -cp $R8_PATH $DUMP_INPUTS com.android.tools.r8.R8 \
    --min-api $MIN_API \
    --lib "$JAVA_HOME" \
    --lib "$ANDROID_JAR" \
    --desugared-lib "$DESUGAR_JDK_LIBS_JSON" \
    --desugared-lib-pg-conf-output desugar_jdk_libs.pgcfg \
    --no-minification \
    --pg-conf playground.pgcfg \
    --pg-map-output Playground.mapping \
    --output . \
    ${EXTRA_JARS/:/ } \
    *.class
$DEXDUMP -d classes.dex > dexdump.txt

rm -f desugar_jdk_libs.dex*
if [[ -n $(cat desugar_jdk_libs.pgcfg) ]]; then
  echo "Running L8"
  $JAVA_BIN/java -cp $R8_PATH com.android.tools.r8.L8 \
      --min-api $MIN_API \
      --lib "$JAVA_HOME" \
      --desugared-lib "$DESUGAR_JDK_LIBS_JSON" \
      --pg-conf desugar_jdk_libs.pgcfg \
      --output desugar_jdk_libs.dex.jar \
      "$DESUGAR_JDK_LIBS_JAR" "$DESUGAR_JDK_LIBS_CONFIGURATION_JAR"
  unzip -p desugar_jdk_libs.dex.jar classes.dex > desugar_jdk_libs.dex
fi
du -b *.dex
echo 'Outputs are: Playground.mapping, classes.dex, dexdump.txt'

