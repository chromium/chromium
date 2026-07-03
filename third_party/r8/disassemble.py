#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import subprocess
import sys

_CUR_DIR = os.path.dirname(os.path.abspath(__file__))
_LIBSUPERSIZE_DIR = os.path.abspath(
    os.path.join(_CUR_DIR, '..', '..', 'tools', 'binary_size', 'libsupersize'))
sys.path.append(_LIBSUPERSIZE_DIR)
import dex_disassembly


def _flush_block(block):
    if not block:
        return
    for l in dex_disassembly.NormalizeLines(block):
        sys.stdout.write(l)
    block.clear()


def main():
    parser = argparse.ArgumentParser(description='Disassemble dex files using R8.')
    parser.add_argument('--normalize', action='store_true', help='Optimize output for diffing')
    args, unknown_args = parser.parse_known_args()

    r8_dir = os.path.dirname(__file__)
    java_path = os.path.abspath(os.path.join(r8_dir, '..', 'jdk', 'current', 'bin', 'java'))
    r8_jar_path = os.path.join(r8_dir, 'cipd', 'lib', 'r8.jar')

    cmd = [java_path, '-cp', r8_jar_path, 'com.android.tools.r8.Disassemble']

    # If no args default to --help.
    if not unknown_args:
        unknown_args = ['--help']

    cmd.extend(unknown_args)

    if args.normalize:
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
        current_block = []
        for line in process.stdout:
            if line.startswith('# Method:'):
                _flush_block(current_block)
            current_block.append(line)
        _flush_block(current_block)
        process.wait()
        sys.exit(process.returncode)
    else:
        # Just exec if not normalizing
        os.execv(java_path, cmd)

if __name__ == '__main__':
    main()
