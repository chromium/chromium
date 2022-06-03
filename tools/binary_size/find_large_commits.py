#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the large commits given a .csv file from a telemetry size graph."""

import argparse
import re
import subprocess


def _ReadCsv(path):
  """Returns the contents of the .csv as a list of (int, int)."""
  ret = []
  with open(path) as f:
    for line in f:
      parts = line.rstrip().split(',')
      if len(parts) == 2 and parts[0] != 'revision':
        ret.append((int(parts[0]), int(float(parts[1]))))
  return ret


def _FindBigDeltas(revs_and_sizes, increase_threshold, decrease_threshold):
  """Filters revs_and_sizes for entries that grow/shrink too much."""
  big_jumps = []
  prev_size = revs_and_sizes[0][1]
  for rev, size in revs_and_sizes:
    delta = size - prev_size
    prev_size = size
    if delta > increase_threshold or -delta > decrease_threshold:
      big_jumps.append((rev, delta))
  return big_jumps


def _LookupCommitInfo(rev):
  sha1 = subprocess.check_output(
      ['git', 'crrev-parse', str(rev)], encoding="utf-8").strip()
  desc = subprocess.check_output(['git', 'log', '-n1', sha1], encoding="utf-8")
  author = re.search(r'Author: .*?<(.*?)>', desc).group(1)
  day, year = re.search(r'Date:\s+\w+\s+(\w+ \d+)\s+.*?\s+(\d+)', desc).groups()
  date = '{} {}'.format(day, year)
  title = re.search(r'\n +(\S.*)', desc).group(1).replace('\t', ' ')
  milestone = None
  if 'Roll AFDO' not in title:
    releases = subprocess.check_output(['git', 'find-releases', sha1],
                                       encoding="utf-8")
    version = re.search('initially in (\d\d)', releases)
    milestone = ''
    if version:
      milestone = 'M{}'.format(version.group(1))
    version = re.search('initially in branch-heads/(\d\d\d\d)', releases)
    if version:
      milestone = version.group(1)

  return sha1, author, date, title, milestone


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--increase-threshold',
      type=int,
      default=50 * 1024,
      help='Minimum number of bytes larger to be considered a notable.')
  parser.add_argument(
      '--decrease-threshold',
      type=int,
      default=30 * 1024,
      help='Minimum number of bytes smaller to be considered a notable.')
  parser.add_argument(
      'points_csv', help='Input .csv file with columns: revision,value')
  options = parser.parse_args()

  revs_and_sizes = _ReadCsv(options.points_csv)
  rev_and_delta = _FindBigDeltas(revs_and_sizes, options.increase_threshold,
                                 options.decrease_threshold)

  print('Printing info for up to {} commits in the range {}-{}'.format(
      len(rev_and_delta), revs_and_sizes[0][0], revs_and_sizes[-1][0]))
  print('Revision,Hash,Title,Author,Delta,Date,Milestone')
  afdo_count = 0
  for rev, delta in rev_and_delta:
    sha1, author, date, title, milestone = _LookupCommitInfo(rev)
    if milestone is not None:
      print('\t'.join(
          [str(rev), sha1, title, author,
           str(delta), date, milestone]))
    else:
      afdo_count += 1
  print('Skipped %d AFDO rolls' % afdo_count)


if __name__ == '__main__':
  main()
