#!/bin/bash
#
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

./dump_process "$1";
cp /proc/"$1"/smaps smaps.txt

