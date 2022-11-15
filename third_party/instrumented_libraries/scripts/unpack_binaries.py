#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unpacks pre-built sanitizer-instrumented third-party libraries."""

import os
import subprocess
import shutil
import sys


def get_archive_name(archive_prefix, release):
  return '%s-%s.tgz' % (archive_prefix, release)


def main(archive_prefix, release, archive_dir, target_dir, stamp_dir=None):
  shutil.rmtree(target_dir, ignore_errors=True)

  os.mkdir(target_dir)
  subprocess.check_call([
      'tar',
      '-zxf',
      os.path.join(archive_dir, get_archive_name(archive_prefix, release)),
      '-C',
      target_dir])
  stamp_file = os.path.join(stamp_dir or target_dir, '%s.txt' % archive_prefix)
  open(stamp_file, 'w').close()

  if stamp_dir:
    with open(os.path.join(stamp_dir, '%s.d' % archive_prefix), 'w') as f:
      f.write('%s: %s' % (
          stamp_file, os.path.join(archive_dir,
                                   get_archive_name(archive_prefix, release))))
  return 0


if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
