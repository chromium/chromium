#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that all the histograms in obsolete histograms XML are obsolete."""

import logging
import os
import sys
import xml.dom.minidom

import extract_histograms
import histogram_paths
import split_xml


def ValidateObsoleteXml():
  """Checks that all the histograms in the obsolete file are obsolete.

  Returns:
    True if at least a histogram is not obsolete, False otherwise.
  """
  has_obsolete_error = False
  tree = xml.dom.minidom.parse(histogram_paths.OBSOLETE_XML)

  for node in extract_histograms.IterElementsWithTag(tree, 'histogram', 3):
    obsolete_tag_nodelist = node.getElementsByTagName('obsolete')
    if len(obsolete_tag_nodelist) > 0:
      continue

    has_obsolete_error = True
    # If histogram is not obsolete, find out the directory that it should be
    # placed in.
    correct_dir = split_xml.GetDirForNode(node)
    histogram_name = node.getAttribute('name')

    logging.error(
        'Histogram of name %s is not obsolete, please move it to the '
        'metadata/%s directory.', histogram_name, correct_dir)

  return has_obsolete_error


if __name__ == '__main__':
  sys.exit(ValidateObsoleteXml())
