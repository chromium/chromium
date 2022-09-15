# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import shutil


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-o', dest='output', required=True)
  parser.add_argument('input')
  args = parser.parse_args()
  shutil.copy(args.input, args.output)


if __name__ == '__main__':
  main()
