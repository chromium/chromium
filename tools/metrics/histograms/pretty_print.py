#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
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

import argparse
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import etree_util
import presubmit_util

import histogram_configuration_model


class Error(Exception):
  pass

UNIT_REWRITES = {
  'mcs': 'microseconds',
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
  if tree.tag == 'histogram':
    units = tree.get('units')
    if units and units in UNIT_REWRITES:
      tree.set('units', UNIT_REWRITES[units])

  for child in tree:
    canonicalizeUnits(child)

def DropNodesByTagName(tree, tag, dropped_nodes=[]):
  """Drop all nodes with named tag from the XML tree."""
  removes = []

  for child in tree:
    if child.tag == tag:
      removes.append(child)
      dropped_nodes.append(child)
    else:
      DropNodesByTagName(child, tag)

  for child in removes:
    tree.remove(child)

def FixMisplacedHistogramsAndHistogramSuffixes(tree):
  """Fixes misplaced histogram and histogram_suffixes nodes."""
  histograms = []
  histogram_suffixes = []

  def ExtractMisplacedHistograms(tree):
    """Gets and drops misplaced histograms and histogram_suffixes.

    Args:
      tree: The node of the xml tree.
      histograms: A list of histogram nodes inside histogram_suffixes_list
          node. This is a return element.
      histogram_suffixes: A list of histogram_suffixes nodes inside histograms
          node. This is a return element.
    """
    for child in tree:
      if child.tag == 'histograms':
        DropNodesByTagName(child, 'histogram_suffixes', histogram_suffixes)
      elif child.tag == 'histogram_suffixes_list':
        DropNodesByTagName(child, 'histogram', histograms)
      else:
        ExtractMisplacedHistograms(child)

  ExtractMisplacedHistograms(tree)

  def AddBackMisplacedHistograms(tree):
    """Adds back those misplaced histogram and histogram_suffixes nodes."""
    for child in tree:
      if child.tag == 'histograms':
        child.extend(histograms)
      elif child.tag == 'histogram_suffixes_list':
        child.extend(histogram_suffixes)
      else:
        AddBackMisplacedHistograms(child)

  AddBackMisplacedHistograms(tree)

def PrettyPrintHistograms(raw_xml):
  """Pretty-print the given histograms XML.

  Args:
    raw_xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  top_level_content = etree_util.GetTopLevelContent(raw_xml)
  root = etree_util.ParseXMLString(raw_xml)
  return top_level_content + PrettyPrintHistogramsTree(root)

def PrettyPrintHistogramsTree(tree):
  """Pretty-print the given ElementTree element.

  Args:
    tree: The ElementTree element.

  Returns:
    The pretty-printed version as an XML string.
  """
  # Prevent accidentally adding enums to histograms.xml
  DropNodesByTagName(tree, 'enums')
  FixMisplacedHistogramsAndHistogramSuffixes(tree)
  canonicalizeUnits(tree)
  return histogram_configuration_model.PrettifyTree(tree)


def PrettyPrintEnums(raw_xml):
  """Pretty print the given enums XML."""

  root = etree_util.ParseXMLString(raw_xml)

  # Prevent accidentally adding histograms to enums.xml
  DropNodesByTagName(root, 'histograms')
  DropNodesByTagName(root, 'histogram_suffixes_list')
  top_level_content = etree_util.GetTopLevelContent(raw_xml)
  formatted_xml = histogram_configuration_model.PrettifyTree(root)
  return top_level_content + formatted_xml

def main():
  """Pretty-prints the histograms or enums xml file at given relative path.

  Args:
    filepath: The relative path to xml file.
    --non-interactive: (Optional) Does not print log info messages and does not
        prompt user to accept the diff.
    --presubmit: (Optional) Simply prints a message if the input is not
        formatted correctly instead of modifying the file.
    --diff: (Optional) Prints diff to stdout rather than modifying the file.

  Example usage:
    pretty_print.py metadata/Fingerprint/histograms.xml
    pretty_print.py enums.xml
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('filepath', help="relative path to XML file")
  # The following optional flags are used by common/presubmit_util.py
  parser.add_argument('--non-interactive', action="store_true")
  parser.add_argument('--presubmit', action="store_true")
  parser.add_argument('--diff', action="store_true")
  args = parser.parse_args()

  status = 0
  if 'enums.xml' in args.filepath:
    status = presubmit_util.DoPresubmit(sys.argv, args.filepath,
                                        'enums.before.pretty-print.xml',
                                        PrettyPrintEnums)

  elif 'histograms' in args.filepath:
    # Specify the individual directory of histograms.xml.
    status = presubmit_util.DoPresubmit(
        sys.argv,
        args.filepath,
        # The backup filename should be
        # 'path/to/histograms.before.pretty-print.xml'.
        '.before.pretty-print.'.join(args.filepath.rsplit('.', 1)),
        PrettyPrintHistograms)

  sys.exit(status)


if __name__ == '__main__':
  main()
