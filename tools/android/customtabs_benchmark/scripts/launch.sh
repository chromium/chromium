#!/bin/sh
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -x

PACKAGE_NAME="org.chromium.customtabs.test"
URL=$1
USE_WEBVIEW=$2
WARMUP=$3

adb root

adb shell am force-stop com.google.android.apps.chrome
adb shell am force-stop $PACKAGE_NAME
adb shell "echo 3 > /proc/sys/vm/drop_caches"

sleep 3

adb shell am start -n ${PACKAGE_NAME}/.MainActivity \
  --es "url" "$URL" \
  --ez "use_webview" "$USE_WEBVIEW" \
  --ez "warmup" "$WARMUP"
