#!/usr/bin/env python
# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from tsc_ignore_errors import run_tsc_ignore_errors

third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

webgpu_cts_src_dir = os.path.join(third_party_dir, 'webgpu-cts', 'src')
src_prefix = webgpu_cts_src_dir.replace('\\', '/') + '/'

def get_ts_sources():
    # This will output all the source files in the form:
    # "/absolute/path/to/file.ts"
    # The path is always Unix-style.
    # It will also output many Typescript errors since the build doesn't download the .d.ts
    # dependencies.
    stdout = run_tsc_ignore_errors([
        '--project',
        os.path.join(webgpu_cts_src_dir, 'tsconfig.json'),
        '--listFiles', '--declaration', 'false', '--sourceMap', 'false'
    ])

    lines = [l.decode() for l in stdout.splitlines()]
    return [
        line[len(src_prefix):] for line in lines
        if line.startswith(src_prefix + 'src/')
    ]


if __name__ == '__main__':
    with open(os.path.join(third_party_dir, 'webgpu-cts', 'ts_sources.txt'),
              'w') as out_file:
        out_file.writelines([x + '\n' for x in get_ts_sources()])
