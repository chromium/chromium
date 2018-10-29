#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pretty-prints the histograms.xml file, alphabetizing tags, wrapping text
at 80 chars, enforcing standard attribute ordering, and standardizing
indentation.

This is quite a bit more complicated than just calling tree.toprettyxml();
we need additional customization, like special attribute ordering in tags
and wrapping text nodes, so we implement our own full custom XML pretty-printer.
"""

from __future__ import with_statement

import logging
import os
import shutil
import sys
import xml.dom.minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import diff_util
import presubmit_util

import histograms_print_style


class Error(Exception):
  pass


UNIT_REWRITES = {
  'microsecond': 'microseconds',
  'us': 'microseconds',
  'millisecond': 'ms',
  'milliseconds': 'ms',
  'kb': 'KB',
  'kB': 'KB',
  'kilobytes': 'KB',
  'kbits/s': 'kbps',
  'mb': 'MB',
  'mB': 'MB',
  'megabytes': 'MB',
  'mbits/s': 'mbps',
  'percent': '%',
  'Percent': '%',
  'percentage': '%',
}


def canonicalizeUnits(tree):
  """Canonicalize the spelling of certain units in histograms."""
  histograms = tree.getElementsByTagName('histogram')
  for histogram in histograms:
    units = histogram.attributes.get('units')
    if units and units.value in UNIT_REWRITES:
      histogram.attributes['units'] = UNIT_REWRITES[units.value]


def fixObsoleteOrder(tree):
  """Put obsolete tags at the beginning of histogram tags."""
  histograms = tree.getElementsByTagName('histogram')
  for histogram in histograms:
    obsoletes = histogram.getElementsByTagName('obsolete')
    if obsoletes:
      histogram.insertBefore(obsoletes[0], histogram.firstChild)


def DropNodesByTagName(tree, tag):
  """Drop all nodes with named tag from the XML tree."""
  nodes = tree.getElementsByTagName(tag)
  for node in nodes:
    node.parentNode.removeChild(node)


def PrettyPrintHistograms(raw_xml):
  """Pretty-print the given XML.

  Args:
    raw_xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  tree = xml.dom.minidom.parseString(raw_xml)
  return PrettyPrintHistogramsTree(tree)


def PrettyPrintHistogramsTree(tree):
  """Pretty-print the given xml.dom.minidom.Document object.

  Args:
    tree: The xml.dom.minidom.Document object.

  Returns:
    The pretty-printed version as an XML string.
  """
  assert isinstance(tree, xml.dom.minidom.Document)
  # Prevent accidentally adding enums to histograms.xml
  DropNodesByTagName(tree, 'enums')
  canonicalizeUnits(tree)
  fixObsoleteOrder(tree)
  return histograms_print_style.GetPrintStyle().PrettyPrintXml(tree)


def PrettyPrintEnums(raw_xml):
  """Pretty print the enums.xml file."""
  tree = xml.dom.minidom.parseString(raw_xml)
  # Prevent accidentally adding histograms to enums.xml
  DropNodesByTagName(tree, 'histograms')
  DropNodesByTagName(tree, 'histogram_suffixes_list')
  return histograms_print_style.GetPrintStyle().PrettyPrintXml(tree)


def main():
  status1 = presubmit_util.DoPresubmit(sys.argv, 'enums.xml',
                                       'enums.before.pretty-print.xml',
                                       'pretty_print.py', PrettyPrintEnums)
  status2 = presubmit_util.DoPresubmit(sys.argv, 'histograms.xml',
                                       'histograms.before.pretty-print.xml',
                                       'pretty_print.py', PrettyPrintHistograms)
  sys.exit(status1 or status2)

if __name__ == '__main__':
  main()
