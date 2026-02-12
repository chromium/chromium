#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verify the `histograms_index` file is up-to-date."""

import logging
import sys

import setup_modules

import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths

def main():
  exit_code = 0
  if not histogram_paths.ValidateHistogramsGniFile():
    exit_code = 1
    logging.error('histograms_xml_files.gni is not up-to-date. Please run '
                  'histogram_paths.py to update it.')
  sys.exit(exit_code)


if __name__ == '__main__':
  main()
