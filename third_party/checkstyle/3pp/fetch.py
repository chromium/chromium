#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(_SRC_ROOT / 'build' / '3pp_common'))
import fetch_github_release


if __name__ == '__main__':
    fetch_github_release.main(project='checkstyle/checkstyle',
                              artifact_filename='checkstyle-all.jar',
                              artifact_regex=r'-all.*\.jar$')
