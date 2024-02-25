#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')))

from fetch_archive import main

_GIT_HOST = 'https://chromium.googlesource.com/chromium/src.git/'

if __name__ == '__main__':
  main(_GIT_HOST + '+log/refs/heads/main/build/fuchsia?n=1&format=json',
       _GIT_HOST + '+archive/{}/build/fuchsia.tar.gz')
