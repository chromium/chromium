# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def main():
  assert sys.argv[-2:] == ['rev-parse', 'HEAD']
  sys.stdout.write('abc123\n')


if __name__ == '__main__':
  main()
