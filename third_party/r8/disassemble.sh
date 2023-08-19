#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

R8DIR=$(dirname $0)
set -x
exec $R8DIR/../jdk/current/bin/java -cp $R8DIR/lib/r8.jar \
  com.android.tools.r8.Disassemble "${@:---help}"
