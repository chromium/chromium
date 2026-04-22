#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import subprocess
import sys

# Instruction prefix: "   0:   0x00: "
PREFIX_RE = re.compile(r'^\s*\d+:\s+0x[0-9a-f]+:\s+')

# Jump targets with absolute and relative offsets: "0x08 (+3)" or "0x3a (+48)"
JUMP_TARGET_RE = re.compile(r'0x[0-9a-f]+\s+\([+-]\d+\)')

# Targets in switches or catch handlers: "-> 0x6e" or "-> 0x10"
TARGET_RE = re.compile(r'->\s+0x[0-9a-f]+')

# Registers like v0, v1, v12, p0, p1, etc.
REGISTER_RE = re.compile(r'\b[vp]\d+\b')


def normalize_line(line):
    if 'PcBasedDebugInfo' in line or line.startswith('~~R8{'):
        return ''
    line = PREFIX_RE.sub('', line)
    line = REGISTER_RE.sub('vN', line)
    line = JUMP_TARGET_RE.sub('<target>', line)
    line = TARGET_RE.sub('-> <target>', line)
    return line


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
        for line in process.stdout:
            sys.stdout.write(normalize_line(line))
        process.wait()
        sys.exit(process.returncode)
    else:
        # Just exec if not normalizing
        os.execv(java_path, cmd)

if __name__ == '__main__':
    main()
