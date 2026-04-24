#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def GetFdCommand():
  """Returns the name of the 'fd' command if it is available and suitable."""
  for cmd in ('fdfind', 'fd'):
    try:
      # We check for --help output to ensure it's the right program and
      # supports the flags we need.
      res = subprocess.run([cmd, '--help'],
                           capture_output=True,
                           text=True,
                           check=False)
      if (res.returncode == 0 and '--glob' in res.stdout
          and '--print0' in res.stdout and '--no-ignore' in res.stdout):
        return cmd
    except OSError:
      continue
  return None


def RemoveIfStale(pyc_path):
  """Deletes pyc_path if it doesn't have a corresponding .py file."""
  dirname, filename = os.path.split(pyc_path)
  if os.path.basename(dirname) == '__pycache__':
    # We have something like 'foo/__pycache__/module.cpython-313.pyc', we want
    # to check if 'foo/module.py' exists.

    # Remove .pyc
    root, _ = os.path.splitext(filename)
    # Remove version indicator like ".cpython-313."
    root, _ = os.path.splitext(root)
    py_path = os.path.join(os.path.dirname(dirname), root + '.py')
  else:
    # We have something like 'foo/module.pyc', we want to check if
    # 'foo/module.py' exists.

    root, _ = os.path.splitext(pyc_path)
    py_path = root + '.py'
  try:
    if not os.path.exists(py_path):
      os.remove(pyc_path)
  except OSError:
    # Wrap OS calls in try/except in case another process touched this file.
    pass


def RemoveAllStalePycFiles(base_dirs):
  """Scan directories for old .pyc files without a .py file and delete them."""
  base_dirs = [d for d in base_dirs if os.path.exists(d)]
  if not base_dirs:
    return
  fd_cmd = GetFdCommand()
  if fd_cmd:
    output = None
    try:
      res = subprocess.run(
          [fd_cmd, '--no-ignore', '--print0', '--glob', '*.pyc'] + base_dirs,
          capture_output=True,
          check=True)
      output = res.stdout
    except (subprocess.CalledProcessError, OSError):
      # Fall back to os.walk if fd fails.
      pass

    if output is not None:
      for pyc_path in output.split(b'\0'):
        if pyc_path:
          RemoveIfStale(os.fsdecode(pyc_path))
      return

  for base_dir in base_dirs:
    for dirname, _, filenames in os.walk(base_dir, topdown=False):
      if '.svn' in dirname or '.git' in dirname:
        continue
      for filename in filenames:
        if filename.endswith('.pyc'):
          RemoveIfStale(os.path.join(dirname, filename))


if __name__ == '__main__':
  RemoveAllStalePycFiles(sys.argv[1:])
