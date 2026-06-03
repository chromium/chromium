#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

def main():
  if len(sys.argv) < 2:
    print("Usage: write_angle_stubs.py <stamp_file> [stub_files...]")
    return 1

  stamp_file = sys.argv[1]
  stub_files = sys.argv[2:]

  # Create the directory for stamp_file if it doesn't exist
  stamp_dir = os.path.dirname(stamp_file)
  if stamp_dir and not os.path.exists(stamp_dir):
    os.makedirs(stamp_dir)

  # Touch the stamp file
  with open(stamp_file, 'w') as f:
    pass

  # Touch all the stub files (empty files)
  for stub in stub_files:
    stub_dir = os.path.dirname(stub)
    if stub_dir and not os.path.exists(stub_dir):
      os.makedirs(stub_dir)
    with open(stub, 'w') as f:
      pass

  return 0

if __name__ == '__main__':
  sys.exit(main())
