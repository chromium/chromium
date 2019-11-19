#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints all histogram names."""

from __future__ import print_function

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import extract_histograms
import histogram_paths
import merge_xml

def main():
  doc = merge_xml.MergeFiles(histogram_paths.ALL_XMLS)
  histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(doc)
  if had_errors:
    raise Error("Error parsing inputs.")
  names = extract_histograms.ExtractNames(histograms)
  for name in names:
    print(name)


if __name__ == '__main__':
  main()
