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
  """Pretty-prints the histograms or enums xml file.

  If no filepath is provided, formats both enums.xml and histograms.xml
  in this script's directory.

  Example usage:
    pretty_print.py
    pretty_print.py metadata/Fingerprint/histograms.xml
    pretty_print.py enums.xml --cleanup
  """
  parser = argparse.ArgumentParser(description=main.__doc__)
  parser.add_argument('filepath',
                      nargs='?',
                      default=None,
                      help="Relative path to XML file. If not specified, "
                      "formats both enums.xml and histograms.xml.")
  # The following optional flags are used by common/presubmit_util.py
  parser.add_argument('--non-interactive', action="store_true")
  parser.add_argument('--presubmit', action="store_true")
  parser.add_argument('--diff', action="store_true")
  parser.add_argument('--cleanup',
                      action="store_true",
                      help="Remove the backup file after a successful run.")
  args = parser.parse_args()

  exit_code = 0

  if 'enums.xml' in args.filepath:
    pretty_fn = PrettyPrintEnums
    backup_filename = 'enums.before.pretty-print.xml'
  elif 'histograms' in args.filepath:
    pretty_fn = PrettyPrintHistograms
    backup_filename = '.before.pretty-print.'.join(args.filepath.rsplit('.', 1))
  else:
    parser.error('File path must contain "enums.xml" or "histograms": %s' %
                 args.filepath)

  status = presubmit_util.DoPresubmit(args, args.filepath, backup_filename,
                                      pretty_fn)
  if status != 0:
    exit_code = status

  sys.exit(exit_code)


if __name__ == '__main__':
  main()
