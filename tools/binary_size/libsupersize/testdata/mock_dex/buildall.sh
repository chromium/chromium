#!/bin/sh
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

MOCK_DEX_DIR=$(realpath $(dirname "$0"))
SRC_DIR=$(realpath $MOCK_DEX_DIR/../../../../..)
JAVAC=$SRC_DIR/third_party/jdk/current/bin/javac
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
popd > /dev/null

echo ''
echo You might need to run the following to update .golden files:
echo ''
echo cd $SRC_DIR/tools/binary_size/libsupersize
echo ./integration_test.py --update
