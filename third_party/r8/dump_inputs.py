#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Convenience script for running proguard.py with --dump-inputs."""

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('mapping_file',
                        help='Path to .mapping file within out/Release/apks.')
    args = parser.parse_args()

    if not args.mapping_file.endswith('.mapping'):
        sys.stderr.write('Expected argument to end with .mapping')
        sys.exit(1)

    # Find directory with "build.ninja" in it.
    build_dir = os.path.dirname(args.mapping_file) or '.'
    for _ in range(50):  # Prevent infinite loop.
        if os.path.exists(os.path.join(build_dir, 'build.ninja')):
            break
        build_dir = os.path.dirname(build_dir) or '.'

    mapping_file = os.path.relpath(args.mapping_file, build_dir)

    command = subprocess.check_output(
        ['ninja', '-C', build_dir, '-t', 'commands', '-s', mapping_file],
        encoding='utf8').rstrip()
    if 'proguard.py' not in command:
        sys.stderr.write('Unexpected: {command}\n')
        sys.exit(1)

    command += ' --dump-inputs'
    print('Running:', command)
    sys.exit(os.system(command))


if __name__ == '__main__':
    main()
