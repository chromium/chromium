#!/usr/bin/env python
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import shutil
import sys
import tempfile

third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from compile_src import compile_src_for_node


def list_testcases(query, js_out_dir=None):
    if js_out_dir is None:
        js_out_dir = tempfile.mkdtemp()
        delete_js_out_dir = True
    else:
        delete_js_out_dir = False

    try:
        logging.info('WebGPU CTS: Transpiling tools...')
        compile_src_for_node(js_out_dir, [
            '--incremental', '--tsBuildInfoFile',
            os.path.join(js_out_dir, 'build.tsbuildinfo')
        ],
                             clean=False)

        old_sys_path = sys.path
        try:
            sys.path = old_sys_path + [os.path.join(third_party_dir, 'node')]
            from node import RunNode
        finally:
            sys.path = old_sys_path

        return RunNode([
            os.path.join(js_out_dir, 'common', 'runtime', 'cmdline.js'), query,
            '--list'
        ])
    finally:
        if delete_js_out_dir:
            shutil.rmtree(js_out_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--query', default='webgpu:*', help='WebGPU CTS Query')
    parser.add_argument(
        '--js-out-dir',
        default=None,
        help='Output directory for intermediate compiled JS sources')
    args = parser.parse_args()

    print(list_testcases(args.query, args.js_out_dir))
