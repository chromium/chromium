#!/usr/bin/env python
# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
node_dir = os.path.join(third_party_dir, 'node')

tsc = os.path.join(node_dir, 'node_modules', 'typescript', 'lib', 'tsc.js')

webgpu_cts_src_dir = os.path.join(third_party_dir, 'webgpu-cts', 'src')
src_prefix = webgpu_cts_src_dir.replace('\\', '/') + '/'

def get_ts_sources():
    sys_path_old = sys.path
    try:
        sys.path = [node_dir] + sys.path
        from node import RunNode

        # This will output all the source files in the form:
        # "/absolute/path/to/file.ts"
        # The path is always Unix-style.
        # It will also output many Typescript errors since the build doesn't
        # download the .d.ts dependencies.
        stdout = RunNode([
            tsc, '--project',
            os.path.join(webgpu_cts_src_dir, 'tsconfig.json'),
            '--listFiles', '--declaration', 'false', '--sourceMap', 'false'
        ])
    finally:
        sys.path = sys_path_old

    return [
        line[len(src_prefix):].rstrip() for line in stdout.split('\n')
        if line.startswith(src_prefix + 'src/')
    ]


if __name__ == '__main__':
    with open(os.path.join(third_party_dir, 'webgpu-cts', 'ts_sources.txt'),
              'w') as out_file:
        out_file.writelines([x + '\n' for x in get_ts_sources()])
