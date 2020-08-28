#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verify the `histograms_index` file is up-to-date."""

import logging
import os
import sys

import histogram_paths

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util


def main():
  exit_code = 0
  with open(histogram_paths.HISTOGRAMS_INDEX, 'r') as f:
    histograms_paths = [os.path.normpath(line.strip()) for line in f]
    if histograms_paths != histogram_paths.HISTOGRAMS_XMLS_RELATIVE:
      exit_code = 1
      logging.error(
        'histograms_index.txt is not up-to-date. Please run '
        'python histogram_paths.py to update it.')

  sys.exit(exit_code)


if __name__ == '__main__':
  main()