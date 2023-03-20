#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import ast
import errno
import tempfile
import os
import shutil
import subprocess
import sys
import logging


third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def check_or_write_file(filepath, content, check):
    if check:
        with open(filepath, 'rb') as dst:
            if dst.read() != content:
                raise RuntimeError(
                    '%s is out of date. Please re-run //third_party/webgpu-cts/scripts/run_regenerate_internal_cts_html.py\n'
                    % filepath)
    else:
        with open(filepath, 'wb') as dst:
            dst.write(content)

def generate_reftest_html(check):
    # Update this to add/remove subdirectories to check.
    sub_dirs = [os.path.join('web_platform', 'reftests')]

    for sub_dir in sub_dirs:
        html_search_dir = os.path.join(third_party_dir, 'webgpu-cts', 'src',
                                       'src', 'webgpu', sub_dir)
        wpt_internal_dir = os.path.join(third_party_dir, 'blink', 'web_tests',
                                        'wpt_internal', 'webgpu', sub_dir)

        if not check:
            shutil.rmtree(wpt_internal_dir)
        logging.info('WebGPU CTS: Generating HTML tests from %s...' % html_search_dir)
        for root, dirnames, filenames in os.walk(html_search_dir):
            for filename in filenames:
                if filename.endswith('.html'):
                    filepath = os.path.join(root, filename)
                    relative_dir = os.path.relpath(os.path.dirname(filepath),
                                                   html_search_dir)
                    dst_dir = os.path.join(wpt_internal_dir, relative_dir)
                    gen_base_dir = os.path.join(
                        '/gen/third_party/webgpu-cts/src/webgpu', sub_dir)
                    if relative_dir != '.':
                        # If we don't do this then tests directly under webplatform/reftests will have ./ in their path
                        gen_base_dir = os.path.join(gen_base_dir, relative_dir)
                    gen_base_dir = gen_base_dir.replace('\\', '/') + '/'

                    try:
                        os.makedirs(dst_dir)
                    except OSError as e:
                        if e.errno == errno.EEXIST and os.path.isdir(dst_dir):
                            pass
                        else:
                            raise

                    with open(filepath, 'rb') as src:
                        src_content = src.read()

                    # Find the starting html tag
                    i = src_content.find(b'<html')
                    assert i != -1

                    # Then find the end of the starting html tag
                    i = src_content.find(b'>', i)
                    assert i != -1

                    # Bump the index just past the starting <html> tag
                    i = i + 1

                    base_tag = b'\n  <base href="%s" />' % gen_base_dir.encode(
                    )
                    dst_content = src_content[:i] + base_tag + src_content[i:]

                    check_or_write_file(os.path.join(dst_dir, filename),
                                        dst_content,
                                        check=check)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check',
                        action='store_true',
                        help='Check that the output file is up to date.')
    parser.add_argument('--stamp', help='Stamp file to write after success.')
    args = parser.parse_args()

    generate_reftest_html(check=args.check)

    if args.stamp:
        with open(args.stamp, 'w') as f:
            f.write('')
