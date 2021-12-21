#!/bin/sh
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

python3 -m unittest discover -s lib -p "*_test.py" $*
