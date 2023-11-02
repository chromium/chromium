#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
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

def get_resource_files():
    files = os.listdir(os.path.join(webgpu_cts_src_dir, 'src', 'resources'))
    files.sort()
    return files

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check',
                        action='store_true',
                        help='Check that the output file is up to date.')
    parser.add_argument('--stamp', help='Stamp file to write after success.')
    args = parser.parse_args()

    ts_sources = [x + '\n' for x in get_ts_sources()]
    ts_sources_txt = os.path.join(third_party_dir, 'webgpu-cts',
                                   'ts_sources.txt')

    resource_files = [x + '\n' for x in get_resource_files()]
    resource_files_txt = os.path.join(third_party_dir, 'webgpu-cts', 'resource_files.txt')

    if args.check:
        with open(ts_sources_txt, 'r') as f:
            if (f.readlines() != ts_sources):
                raise RuntimeError(
                    '%s is out of date. Please re-run //third_party/webgpu-cts/scripts/gen_ts_dep_lists.py\n'
                    % ts_sources_txt)
        with open(resource_files_txt, 'r') as f:
            if (f.readlines() != resource_files):
                raise RuntimeError(
                    '%s is out of date. Please re-run //third_party/webgpu-cts/scripts/gen_ts_dep_lists.py\n'
                    % resource_files_txt)
    else:
        with open(ts_sources_txt, 'w') as f:
            f.writelines(ts_sources)
        with open(resource_files_txt, 'w') as f:
            f.writelines(resource_files)

    if args.stamp:
        with open(args.stamp, 'w') as f:
            f.write('')
