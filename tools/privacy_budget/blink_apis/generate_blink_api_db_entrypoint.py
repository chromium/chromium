#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import subprocess
import sys
import os

if __name__ == "__main__":
  shell = (os.name == 'nt')
  subprocess.call([
      "vpython",
      os.path.join(os.path.dirname(os.path.realpath(__file__)),
                   "generate_blink_api_db.py")
  ] + sys.argv[1:], shell=shell)
