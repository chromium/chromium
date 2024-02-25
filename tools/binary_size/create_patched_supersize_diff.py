#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Diffs two .size files and them merges .sizediff files."""

import argparse
import os
import subprocess


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', required=True)
  parser.add_argument('before_size')
  parser.add_argument('load_size')
  parser.add_argument('sizediff_files', nargs='+')
  args = parser.parse_args()

  # Make sure files were downloaded from Tiger Viewer correctly.
  assert args.before_size.endswith('before_size.size')
  assert args.load_size.endswith('load_size.size')
  assert args.output.endswith('.sizediff')
  assert all(p.endswith('.sizediff') for p in args.sizediff_files)

  script = ['d = Diff(size_info1, size_info2)']
  for i in range(len(args.sizediff_files)):
    i = i * 2 + 4  # 4, 6, 8, ...
    script += [f'd.MergeDeltaSizeInfo(Diff(size_info{i}, size_info{i - 1}))']
  script += [f'SaveDeltaSizeInfo(d, {repr(args.output)})']

  supersize = os.path.join(os.path.dirname(__file__), 'supersize')
  cmd = [supersize, 'console', args.before_size, args.load_size]
  cmd += args.sizediff_files
  cmd += ['--query', ';'.join(script)]
  subprocess.run(cmd, check=True)


if __name__ == '__main__':
  main()
