#!/bin/sh
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

MOCK_DEX_DIR=$(realpath $(dirname "$0"))
MOCK_SDK_TOOLS_BIN_DIR=$(realpath $MOCK_DEX_DIR/../mock_sdk/tools/bin)
SRC_DIR=$(realpath $MOCK_DEX_DIR/../../../../..)
JAVAC=$SRC_DIR/third_party/jdk/current/bin/javac
APKANALYZER=$SRC_DIR/third_party/android_build_tools/apkanalyzer/apkanalyzer
D8=$SRC_DIR/third_party/android_sdk/public/build-tools/33.0.0/d8

pushd $MOCK_DEX_DIR/before > /dev/null
echo Making $MOCK_DEX_DIR/before/classes.dex
$JAVAC TestClass.java
$D8 TestClass.class
rm TestClass.class
popd > /dev/null

pushd $MOCK_DEX_DIR/after > /dev/null
echo Making $MOCK_DEX_DIR/after/classes.dex
$JAVAC TestClass.java
$D8 TestClass.class AuxClass.class
rm TestClass.class AuxClass.class
echo Making $MOCK_SDK_TOOLS_BIN_DIR/apkanalyzer.output
zip temp.apk classes.dex > /dev/null
$APKANALYZER dex packages temp.apk > $MOCK_SDK_TOOLS_BIN_DIR/apkanalyzer.output
rm temp.apk
popd > /dev/null

echo ''
echo You might need to run the following to update .golden files:
echo ''
echo cd $SRC_DIR/tools/binary_size/libsupersize
echo ./integration_test.py --update
