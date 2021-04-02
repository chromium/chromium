#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

from tsc_ignore_errors import run_tsc_ignore_errors

webgpu_cts_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
third_party_dir = os.path.dirname(webgpu_cts_dir)
node_dir = os.path.join(third_party_dir, 'node')

try:
    old_sys_path = sys.path
    sys.path = [node_dir] + sys.path

    from node import RunNode
finally:
    sys.path = old_sys_path

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: compile_src.py GEN_DIR')
        sys.exit(1)

    gen_dir = sys.argv[1]

    # Compile the CTS src.
    run_tsc_ignore_errors([
        '--project',
        os.path.join(webgpu_cts_dir, 'src', 'tsconfig.json'),
        '--outDir',
        os.path.join(gen_dir, 'src'),
        '--noEmit',
        'false',
        '--declaration',
        'false',
        '--sourceMap',
        'false',
        '--target',
        'ES2017',
    ])

    # Compile the gen_listings tool for Node.js
    run_tsc_ignore_errors([
        os.path.join(webgpu_cts_dir, 'src', 'src', 'common', 'tools',
                     'gen_listings.ts'),
        '--outDir',
        os.path.join(gen_dir, 'node'),
        '--declaration',
        'false',
        '--sourceMap',
        'false',
    ])

    # Run gen_listings.js to overwrite the dummy src/webgpu/listings.js created
    # from transpiling src/
    RunNode([
        os.path.join(gen_dir, 'node', 'tools', 'gen_listings.js'),
        '--no-validate',
        os.path.join(gen_dir, 'src'),
        os.path.join(gen_dir, 'src', 'webgpu'),
    ])
