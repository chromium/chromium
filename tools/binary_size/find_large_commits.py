#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the large commits given a .csv file from a telemetry size graph."""

import argparse
import re
import subprocess


# Commit ranges where perf bot was giving invalid results.
# Range objects implement __contains__ for fast "in" operators.
_BAD_COMMIT_RANGES = [
    range(1045024, 1045552),  # https://crbug.com/1361952
]


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
  prev_rev, prev_size = revs_and_sizes[0]
  for rev, size in revs_and_sizes:
    delta = size - prev_size
    if delta > increase_threshold or -delta > decrease_threshold:
      big_jumps.append((rev, delta, prev_rev))
    prev_rev = rev
    prev_size = size
  return big_jumps


def _LookupCommitInfo(rev):
  sha1 = subprocess.check_output(
      ['git', 'crrev-parse', str(rev)], encoding="utf-8").strip()
  if not sha1:
    raise Exception(f'git crrev-parse for {rev} failed. Probably need to '
                    f'"git fetch origin main"')
  desc = subprocess.check_output(['git', 'log', '-n1', sha1], encoding="utf-8")
  author = re.search(r'Author: .*?<(.*?)>', desc).group(1)
  day, year = re.search(r'Date:\s+\w+\s+(\w+ \d+)\s+.*?\s+(\d+)', desc).groups()
  date = '{} {}'.format(day, year)
  title = re.search(r'\n +(\S.*)', desc).group(1).replace('\t', ' ')
  return sha1, author, date, title


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--increase-threshold',
      type=int,
      default=30 * 1024,
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
  big_deltas = _FindBigDeltas(revs_and_sizes, options.increase_threshold,
                              options.decrease_threshold)

  print('Printing info for up to {} commits in the range {}-{}'.format(
      len(big_deltas), revs_and_sizes[0][0], revs_and_sizes[-1][0]))
  print('Revision,Hash,Title,Author,Delta,Date')
  num_bad_commits = 0
  for rev, delta, prev_rev in big_deltas:
    if any(rev in r for r in _BAD_COMMIT_RANGES):
      num_bad_commits += 1
      continue
    sha1, author, date, title = _LookupCommitInfo(rev)
    rev_str = str(rev)
    if rev - prev_rev > 1:
      rev_str = f'{prev_rev}..{rev}'
    print('\t'.join([rev_str, sha1, title, author, str(delta), date]))

  if num_bad_commits:
    print(f'Ignored {num_bad_commits} commits from bad ranges')


if __name__ == '__main__':
  main()
