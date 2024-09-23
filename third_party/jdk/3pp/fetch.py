#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import sys

_3PP_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _3PP_DIR.parents[2]
sys.path.insert(0, str(_SRC_ROOT / 'build' / '3pp_common'))
import fetch_github_release


if __name__ == '__main__':
    fetch_github_release.main(project='adoptium/temurin21-binaries',
                              artifact_extension='.tar.gz',
                              artifact_regex=r'jdk_x64_linux_.*\.tar\.gz$',
                              install_scripts=[_3PP_DIR / 'install.sh'])
