#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR))))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if not TYP_DIR in sys.path:
  sys.path.insert(0, TYP_DIR)

import typ

sys.exit(typ.main(top_level_dir=THIS_DIR, suffixes=['*_test.py']))
