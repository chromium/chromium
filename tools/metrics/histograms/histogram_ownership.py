#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple tool to go through histograms.xml and print out the owners for
histograms.
"""

from __future__ import print_function

import extract_histograms
import os
import sys
import xml.etree.ElementTree

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

def main():
  tree = xml.etree.ElementTree.parse(path_util.GetHistogramsFile())
  root = tree.getroot()
  assert root.tag == 'histogram-configuration'

  root_children = root.getchildren()
  histograms = None
  for node in root_children:
    if node.tag == 'histograms':
      histograms = node
      break
  assert histograms != None

  for histogram in histograms.getchildren():
    if histogram.tag != 'histogram':
      continue

    name = histogram.attrib['name']
    owners = []
    obsolete = False
    for node in histogram.getchildren():
      if node.tag == 'obsolete':
        obsolete = True
        continue
      if node.tag != 'owner':
        continue
      if node.text == extract_histograms.OWNER_PLACEHOLDER:
        continue
      assert '@' in node.text
      owners.append(node.text)

    if not obsolete:
      if owners:
        print(name, ' '.join(owners))
      else:
        print(name, 'NO_OWNER')


if __name__ == '__main__':
  main()
