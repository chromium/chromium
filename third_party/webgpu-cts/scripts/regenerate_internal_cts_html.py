#!/usr/bin/env vpython
# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
import tempfile
import os
import shutil
import subprocess
import sys

third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def generate_internal_cts_html():
    split_list_fd, split_list_fname = tempfile.mkstemp()
    cts_html_fd, cts_html_fname = tempfile.mkstemp()
    js_out_dir = tempfile.mkdtemp()
    try:
        print('WebGPU CTS: Extracting expectation names...')
        old_sys_path = sys.path
        try:
            sys.path = old_sys_path + [
                os.path.join(third_party_dir, 'blink', 'tools')
            ]

            from run_webgpu_cts import split_cts_expectations_and_web_test_expectations
        finally:
            sys.path = old_sys_path

        with open(
                os.path.join(third_party_dir, 'blink', 'web_tests',
                             'WebGPUExpectations')) as f:
            web_test_expectations = split_cts_expectations_and_web_test_expectations(
                f.read())['web_test_expectations']['expectations']

        print('WebGPU CTS: Reading manual test splits...')
        with open(
                os.path.join(third_party_dir, 'blink', 'web_tests', 'webgpu',
                             'internal_cts_test_splits.pyl')) as f:
            manual_splits = ast.literal_eval(f.read())

        with open(split_list_fname, 'w') as split_list_out:
            for expectation in web_test_expectations:
                if expectation.test:
                    split_list_out.write('%s\n' % expectation.test)
            for test in manual_splits:
                split_list_out.write('%s\n' % test)

        print('WebGPU CTS: Transpiling tools...')
        cmd = [
            '../scripts/tsc_ignore_errors.py',
            '--project',
            'node.tsconfig.json',
            '--outDir',
            js_out_dir,
            '--noEmit',
            'false',
            '--declaration',
            'false',
            '--sourceMap',
            'false',
        ]
        process = subprocess.Popen(cmd,
                                   cwd=os.path.join(third_party_dir,
                                                    'webgpu-cts', 'src'))
        process.communicate()

        print('WebGPU CTS: Generating cts.html contents...')
        cmd = [
            os.path.join(third_party_dir, 'node', 'node.py'),
            os.path.join(js_out_dir,
                         'common/tools/gen_wpt_cts_html.js'), cts_html_fname,
            os.path.join(third_party_dir, 'blink', 'web_tests', 'webgpu',
                         'ctshtml-template.txt'),
            os.path.join(third_party_dir, 'blink', 'web_tests', 'webgpu',
                         'argsprefixes.txt'), split_list_fname,
            'wpt_internal/webgpu/cts.html', 'webgpu'
        ]
        process = subprocess.Popen(cmd)
        process.communicate()

        with open(cts_html_fname) as f:
            return f.read()

    finally:
        os.close(split_list_fd)
        os.close(cts_html_fd)
        shutil.rmtree(js_out_dir)


if __name__ == '__main__':
    out_cts_html = os.path.join(third_party_dir, 'blink', 'web_tests',
                                'wpt_internal', 'webgpu', 'cts.html')
    contents = generate_internal_cts_html()
    if not contents:
        print('Failed to generate %s' % out_cts_html)
        sys.exit(1)

    with open(out_cts_html, 'wb') as f:
        f.write(contents)
