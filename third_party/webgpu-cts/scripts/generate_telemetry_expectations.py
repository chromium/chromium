#!/usr/bin/env python
# Copyright 2022 The Chromium Authors. All rights reserved.
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


def generate_telemetry_expectations(cts_expectation_queries,
                                    expectations_out,
                                    js_out_dir=None):
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

        args = [
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         'generate_telemetry_expectations.js'),
            os.path.abspath(js_out_dir),
            os.path.abspath(cts_expectation_queries),
        ]
        if expectations_out:
            args.append(os.path.abspath(expectations_out))

        return RunNode(args)
    finally:
        if delete_js_out_dir:
            shutil.rmtree(js_out_dir)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('cts_expectation_queries',
                        help="Path to the CTS expectation queries")
    parser.add_argument(
        '--expectations-out',
        default=None,
        help="Path to output expectations. If not passed, prints to stdout.")
    parser.add_argument(
        '--js-out-dir',
        default=None,
        help='Output directory for intermediate compiled JS sources')
    args = parser.parse_args()

    print(
        generate_telemetry_expectations(args.cts_expectation_queries,
                                        args.expectations_out,
                                        args.js_out_dir))
