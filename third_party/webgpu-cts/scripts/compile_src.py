#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import platform
import subprocess
import sys
import os

third_party_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
node_dir = os.path.join(third_party_dir, 'node')

tsc = os.path.join(node_dir, 'node_modules', 'typescript', 'lib', 'tsc.js')

def get_node_binary_path():
  return os_path.join(node_dir, *{
    'Darwin': ('mac', 'node-darwin-x64', 'bin', 'node'),
    'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
    'Windows': ('win', 'node.exe'),
  }[platform.system()])

def run_tsc_ignore_errors(args):
  cmd = [get_node_binary_path(), tsc] + args
  process = subprocess.Popen(
    cmd, cwd=os.getcwd(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  stdout, stderr = process.communicate()

  # Typecheck errors go in stdout, not stderr. If we see something in stderr, raise an error.
  if len(stderr):
    raise RuntimeError('tsc \'%s\' failed\n%s' % (' '.join(cmd), stderr))

  return stdout

if __name__ == '__main__':
  run_tsc_ignore_errors(sys.argv[1:])
