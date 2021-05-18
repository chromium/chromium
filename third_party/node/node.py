#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from os import path as os_path
import platform
import subprocess
import sys
import os


def GetBinaryPath():
  # TODO: Node 16.0 will likely ship with an official universal node binary
  # on macOS. Once node 16.0 is released, remove this special case here
  # and use node-darwin-universal in the dict in the main return statement.
  if platform.system() == 'Darwin' and platform.machine() == 'arm64':
      return os.path.join(os_path.join(os_path.dirname(__file__), 'mac',
                          'node-darwin-arm64', 'bin', 'node'))
  return os_path.join(os_path.dirname(__file__), *{
    'Darwin': ('mac', 'node-darwin-x64', 'bin', 'node'),
    'Linux': ('linux', 'node-linux-x64', 'bin', 'node'),
    'Windows': ('win', 'node.exe'),
  }[platform.system()])


def RunNode(cmd_parts, stdout=None):
  cmd = [GetBinaryPath()] + cmd_parts
  process = subprocess.Popen(
      cmd, cwd=os.getcwd(), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
      universal_newlines=True)
  stdout, stderr = process.communicate()

  if process.returncode != 0:
    # Handle cases where stderr is empty, even though the command failed, for
    # example https://github.com/microsoft/TypeScript/issues/615
    err = stderr if len(stderr) > 0 else stdout
    raise RuntimeError('Command \'%s\' failed\n%s' % (' '.join(cmd), err))

  return stdout

if __name__ == '__main__':
  RunNode(sys.argv[1:])
