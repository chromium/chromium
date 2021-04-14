#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that the histograms XML file is well-formatted."""

import os
import sys

import extract_histograms
import histogram_paths
import merge_xml

def main():
  doc = merge_xml.MergeFiles(histogram_paths.ALL_XMLS,
                             should_expand_owners=True)
  _, errors = extract_histograms.ExtractHistogramsFromDom(doc)
  sys.exit(errors)

if __name__ == '__main__':
  main()
