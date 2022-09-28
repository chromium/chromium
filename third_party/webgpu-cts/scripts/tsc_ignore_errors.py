#!/usr/bin/env python
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys
import os

third_party_dir = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
node_dir = os.path.join(third_party_dir, 'node')

try:
    old_sys_path = sys.path
    sys.path = [node_dir] + sys.path

    from node import GetBinaryPath as get_node_binary_path
finally:
    sys.path = old_sys_path

tsc = os.path.join(node_dir, 'node_modules', 'typescript', 'lib', 'tsc.js')


def run_tsc_ignore_errors(args):
    cmd = [get_node_binary_path(), tsc] + args
    process = subprocess.Popen(cmd,
                               cwd=os.getcwd(),
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)

    stdout, stderr = process.communicate()

    # Typecheck errors go in stdout, not stderr. If we see something in stderr, raise an error.
    if len(stderr):
        raise RuntimeError('tsc \'%s\' failed\n%s' % (' '.join(cmd), stderr))

    return stdout


if __name__ == '__main__':
    run_tsc_ignore_errors(sys.argv[1:])
