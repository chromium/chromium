#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

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


def compile_src(out_dir):
    # First, clean the output directory so deleted files are pruned from old builds.
    shutil.rmtree(out_dir)

    run_tsc_ignore_errors([
        '--project',
        os.path.join(webgpu_cts_dir, 'src', 'tsconfig.json'),
        '--outDir',
        out_dir,
        '--noEmit',
        'false',
        '--noEmitOnError',
        'false',
        '--declaration',
        'false',
        '--sourceMap',
        'false',
        '--target',
        'ES2017',
    ])


def compile_src_for_node(out_dir, additional_args=None, clean=True):
    additional_args = additional_args or []
    if clean:
        # First, clean the output directory so deleted files are pruned from old builds.
        shutil.rmtree(out_dir)

    args = [
        '--project',
        os.path.join(webgpu_cts_dir, 'src', 'node.tsconfig.json'),
        '--outDir',
        out_dir,
        '--noEmit',
        'false',
        '--noEmitOnError',
        'false',
        '--declaration',
        'false',
        '--sourceMap',
        'false',
        '--target',
        'ES6',
    ]
    args.extend(additional_args)

    run_tsc_ignore_errors(args)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: compile_src.py GEN_DIR')
        sys.exit(1)

    gen_dir = sys.argv[1]

    # Compile the CTS src.
    compile_src(os.path.join(gen_dir, 'src'))
    compile_src_for_node(os.path.join(gen_dir, 'src-node'))

    # Run gen_listings.js to overwrite the dummy src/webgpu/listings.js created
    # from transpiling src/
    RunNode([
        os.path.join(gen_dir, 'src-node', 'common', 'tools',
                     'gen_listings.js'),
        '--no-validate',
        os.path.join(gen_dir, 'src'),
        os.path.join(gen_dir, 'src-node', 'webgpu'),
    ])
