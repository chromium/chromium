#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import pathlib
import shutil
import subprocess
import sys


def should_ignore(path: pathlib.Path):
  with pathlib.Path('.yapfignore').open() as f:
    for pattern in f:
      pattern = pattern.strip()
      # Comments and blank patterns should be ignored.
      if pattern.startswith('#') or pattern == '':
        continue
      if fnmatch.fnmatch(path, pattern):
        return True
  return False


def get_style(path: pathlib.Path):
  for parent in path.parents:
    fname = parent / '.style.yapf'
    if fname.is_file():
      return str(fname)
  return 'pep8'


def main(path: pathlib.Path):
  if should_ignore(path):
    shutil.copyfileobj(sys.stdin, sys.stdout)
  else:
    yapf = shutil.which('yapf')
    args = [yapf, '--style', get_style(path)]
    if sys.platform == 'win32':
      sys.exit(subprocess.call(args))
    else:
      os.execv(yapf, args)


if __name__ == '__main__':
  main(pathlib.Path(sys.argv[1]))
