#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints all histogram names."""

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import tempfile

try:
  from StringIO import StringIO  # for Python 2
except ImportError:
  from io import StringIO  # for Python 3

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import extract_histograms
import histogram_paths
import merge_xml


def get_names(xml_files):
  doc = merge_xml.MergeFiles(files=xml_files)
  histograms, had_errors = extract_histograms.ExtractHistogramsFromDom(doc)
  if had_errors:
    raise ValueError("Error parsing inputs.")
  return set(extract_histograms.ExtractNames(histograms))


def histogram_xml_files():
  return [open(f) for f in histogram_paths.ALL_XMLS]


def get_diff(revision):
  """Returns the added / removed histogram names relative to git revision

  Args:
    revision: A git revision as described in
      https://git-scm.com/docs/gitrevisions
  Returns:
    A tuple of (added names, removed names), where each entry is sorted in
    ascending order.
  """

  def get_file_at_revision(path):
    """Returns a file-like object containing |path|'s content at |revision|"""
    obj = "%s:%s" % (revision, path)
    contents = subprocess.check_output(
        ("git", "cat-file", "--textconv", obj)).decode()

    # Just store the contents in memory. histograms.xml is big, but it isn't
    # _that_ big.
    return StringIO(contents)

  prev_files = []
  for p in histogram_paths.ALL_XMLS_RELATIVE:
    try:
      prev_files.append(get_file_at_revision(os.path.normpath(p)))
    except subprocess.CalledProcessError:
      # Paths might not exist in the provided revision.
      continue

  current_histogram_names = get_names(histogram_xml_files())
  prev_histogram_names = get_names(prev_files)

  added_names = sorted(list(current_histogram_names - prev_histogram_names))
  removed_names = sorted(list(prev_histogram_names - current_histogram_names))
  return (added_names, removed_names)


def print_diff_names(revision):
  added_names, removed_names = get_diff(revision)
  print("%d histograms added:" % len(added_names))
  for name in added_names:
    print(name)

  print("%d histograms removed:" % len(removed_names))
  for name in removed_names:
    print(name)


def main(argv):
  parser = argparse.ArgumentParser(description='Print histogram names.')
  parser.add_argument('--diff',
                      type=str,
                      help='Git revision to diff against (e.g. HEAD~)')
  args = parser.parse_args(argv[1:])
  if args.diff is not None:
    print_diff_names(args.diff)
  else:
    for name in get_names(histogram_xml_files()):
      print(name)


if __name__ == '__main__':
  main(sys.argv)
