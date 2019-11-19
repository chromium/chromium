#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares two orderfiles, from filenames or a commit.

This shows some statistics about two orderfiles, possibly extracted from an
updating commit made by the orderfile bot.
"""

from __future__ import print_function

import argparse
import collections
import logging
import os
import subprocess
import sys


def ParseOrderfile(filename):
  """Parses an orderfile into a list of symbols.

  Args:
    filename: (str) Path to the orderfile.

  Returns:
    [str] List of symbols.
  """
  symbols = []
  lines = []
  already_seen = set()
  with open(filename, 'r') as f:
    lines = [line.strip() for line in f]

  # The (new) orderfiles that are oriented at the LLD linker contain only symbol
  # names (i.e. not prefixed with '.text'). The (old) orderfiles aimed at the
  # Gold linker were patched by duplicating symbols prefixed with '.text.hot.',
  # '.text.unlikely.' and '.text.', hence the appearance of '.text' on the first
  # symbol indicates such a legacy orderfile.
  if not lines[0].startswith('.text.'):
    for entry in lines:
      symbol_name = entry.rstrip('\n')
      assert symbol_name != '*' and symbol_name != '.text'
      already_seen.add(symbol_name)
      symbols.append(symbol_name)
  else:
    for entry in lines:
      # Keep only (input) section names, not symbol names (only rare special
      # symbols contain '.'). We could only keep symbols, but then some even
      # older orderfiles would not be parsed.
      if '.' not in entry:
        continue
      # Example: .text.startup.BLA
      symbol_name = entry[entry.rindex('.'):]
      if symbol_name in already_seen or symbol_name == '*' or entry == '.text':
        continue
      already_seen.add(symbol_name)
      symbols.append(symbol_name)
  return symbols


def CommonSymbolsToOrder(symbols, common_symbols):
  """Returns s -> index for all s in common_symbols."""
  result = {}
  index = 0
  for s in symbols:
    if s not in common_symbols:
      continue
    result[s] = index
    index += 1
  return result


CompareResult = collections.namedtuple(
    'CompareResult', ('first_count', 'second_count',
                      'new_count', 'removed_count',
                      'average_fractional_distance'))

def Compare(first_filename, second_filename):
  """Outputs a comparison of two orderfiles to stdout.

  Args:
    first_filename: (str) First orderfile.
    second_filename: (str) Second orderfile.

  Returns:
    An instance of CompareResult.
  """
  first_symbols = ParseOrderfile(first_filename)
  second_symbols = ParseOrderfile(second_filename)
  print('Symbols count:\n\tfirst:\t%d\n\tsecond:\t%d' % (len(first_symbols),
                                                         len(second_symbols)))
  first_symbols = set(first_symbols)
  second_symbols = set(second_symbols)
  new_symbols = second_symbols - first_symbols
  removed_symbols = first_symbols - second_symbols
  common_symbols = first_symbols & second_symbols
  # Distance between orderfiles.
  first_to_ordering = CommonSymbolsToOrder(first_symbols, common_symbols)
  second_to_ordering = CommonSymbolsToOrder(second_symbols, common_symbols)
  total_distance = sum(abs(first_to_ordering[s] - second_to_ordering[s])\
                       for s in first_to_ordering)
  # Each distance is in [0, len(common_symbols)] and there are
  # len(common_symbols) entries, hence the normalization.
  average_fractional_distance = float(total_distance) / (len(common_symbols)**2)
  print('New symbols = %d' % len(new_symbols))
  print('Removed symbols = %d' % len(removed_symbols))
  print('Average fractional distance = %.2f%%' %
        (100. * average_fractional_distance))
  return CompareResult(len(first_symbols), len(second_symbols),
                       len(new_symbols), len(removed_symbols),
                       average_fractional_distance)


def CheckOrderfileCommit(commit_hash, clank_path):
  """Asserts that a commit is an orderfile update from the bot.

  Args:
    commit_hash: (str) Git hash of the orderfile roll commit.
    clank_path: (str) Path to the clank repository.
  """
  output = subprocess.check_output(
      ['git', 'show', r'--format=%an %s', commit_hash], cwd=clank_path)
  first_line = output.split('\n')[0]
  # Capitalization changed at some point.
  assert first_line.upper() == 'clank-autoroller Update Orderfile.'.upper(), (
      'Not an orderfile commit')


def GetBeforeAfterOrderfileHashes(commit_hash, clank_path):
  """Downloads the orderfiles before and afer an orderfile roll.

  Args:
    commit_hash: (str) Git hash of the orderfile roll commit.
    clank_path: (str) Path to the clank repository.

  Returns:
    (str, str) Path to the before and after commit orderfiles.
  """
  orderfile_hash_relative_path = 'orderfiles/orderfile.arm.out.sha1'
  before_output = subprocess.check_output(
      ['git', 'show', '%s^:%s' % (commit_hash, orderfile_hash_relative_path)],
      cwd=clank_path)
  before_hash = before_output.split('\n')[0]
  after_output = subprocess.check_output(
      ['git', 'show', '%s:%s' % (commit_hash, orderfile_hash_relative_path)],
      cwd=clank_path)
  after_hash = after_output.split('\n')[0]
  assert before_hash != after_hash
  return (before_hash, after_hash)


def DownloadOrderfile(orderfile_hash, output_filename):
  """Downloads an orderfile with a given hash to a given destination."""
  cloud_storage_path = (
      'gs://clank-archive/orderfile-clankium/%s' % orderfile_hash)
  subprocess.check_call(
      ['gsutil.py', 'cp', cloud_storage_path, output_filename])


def GetOrderfilesFromCommit(commit_hash):
  """Returns paths to the before and after orderfiles for a commit."""
  clank_path = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                            'clank')
  logging.info('Checking that the commit is an orderfile')
  CheckOrderfileCommit(commit_hash, clank_path)
  (before_hash, after_hash) = GetBeforeAfterOrderfileHashes(
      commit_hash, clank_path)
  logging.info('Before / after hashes: %s %s', before_hash, after_hash)
  before_filename = os.path.join('/tmp/', before_hash)
  after_filename = os.path.join('/tmp/', after_hash)
  logging.info('Downloading files')
  DownloadOrderfile(before_hash, before_filename)
  DownloadOrderfile(after_hash, after_filename)
  return (before_filename, after_filename)


def CreateArgumentParser():
  """Returns the argumeng parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--first', help='First orderfile')
  parser.add_argument('--second', help='Second orderfile')
  parser.add_argument('--keep', default=False, action='store_true',
                      help='Keep the downloaded orderfiles')
  parser.add_argument('--from-commit', help='Analyze the difference in the '
                      'orderfile from an orderfile bot commit.')
  parser.add_argument('--csv-output', help='Appends the result to a CSV file.')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = CreateArgumentParser()
  args = parser.parse_args()
  if args.first or args.second:
    assert args.first and args.second, 'Need both files.'
    Compare(args.first, args.second)
  elif args.from_commit:
    first, second = GetOrderfilesFromCommit(args.from_commit)
    try:
      logging.info('Comparing the orderfiles')
      result = Compare(first, second)
      if args.csv_output:
        with open(args.csv_output, 'a') as f:
          f.write('%s,%d,%d,%d,%d,%f\n' % tuple(
              [args.from_commit] + list(result)))
    finally:
      if not args.keep:
        os.remove(first)
        os.remove(second)
  else:
    return False
  return True


if __name__ == '__main__':
  sys.exit(0 if main() else 1)
