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
    parser.add_argument('output_file',
                        help='Path to .dex.jar or .mapping file within out/')
    parser.add_argument('--build',
                        action='store_true',
                        help='Whether to try compile first.')
    args = parser.parse_args()

    # Find directory with "build.ninja" in it.
    build_dir = os.path.dirname(args.output_file) or '.'
    for _ in range(50):  # Prevent infinite loop.
        if os.path.exists(os.path.join(build_dir, 'build.ninja')):
            break
        build_dir = os.path.dirname(build_dir) or '.'

    output_file = os.path.relpath(args.output_file, build_dir)

    if args.build:
        # autoninja so it builds under RBE
        build_cmd = ['autoninja', '-C', build_dir, output_file]
        print('Running:', ' '.join(build_cmd))
        subprocess.run(build_cmd)

    command = subprocess.check_output(
        ['ninja', '-C', build_dir, '-t', 'commands', '-s', output_file],
        encoding='utf8').rstrip()
    if 'proguard.py' not in command and 'dex.py' not in command:
        sys.stderr.write('Unexpected: {command}\n')
        sys.exit(1)

    command += ' --dump-inputs'
    print('Running:', command)
    # Ninja commands are meant to be run from within the build directory.
    os.chdir(build_dir)
    sys.exit(os.system(command))


if __name__ == '__main__':
    main()
