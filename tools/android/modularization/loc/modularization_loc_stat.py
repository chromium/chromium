#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates stats on modularization efforts. Stats include:
- Percentage of added lines in modularized files over legacy ones.
- The top 50 contributors to the modularized files.
"""

import argparse
import datetime
import json
import os
import subprocess
import sys
from collections import OrderedDict
from collections import defaultdict
from typing import List, Tuple

# Output json keys
KEY_LOC_MODULARIZED = 'loc_modularized'
KEY_LOC_LEGACY = 'loc_legacy'
KEY_RANKINGS_MODULARIZED = 'rankings'
KEY_RANKINGS_LEGACY = 'rankings_legacy'
KEY_START_DATE = 'start_date'
KEY_END_DATE = 'end_date'

_M12N_DIRS = [
    'chrome/browser',
    'components',
]

_LEGACY_DIR = 'chrome/android'


def GenerateLOCStats(start_date,
                     end_date,
                     *,
                     quiet=False,
                     json_format=False,
                     git_dir=None):
  """Generate modulazation LOC stats.

  Args:
    start_date: The date to analyze the stat from.
    end_date: The date to analyze the stat to.
    quiet: True if no message is output during the processing.
    json_format: True if the output should be in json format. Otherwise
        a plain, human-readable table is generated.
    git_dir: Git repo directory to use for stats. If None, the current directory
        is used.

  Return:
    Text string containing the stat in a specified format.
  """

  #  Each CL is output in the following format:
  #
  #   :thanhdng:2020-08-17:Use vector icons for zero state file results
  #
  #  118     98      chromeos/ui/base/file_icon_util.cc
  #  2       1       chromeos/ui/base/file_icon_util.h
  #  0       20      chromeos/ui/base/file_icon_util_unittest.cc
  #
  #  i.e.:
  #
  #   :author:commit-date:subject
  #
  #  added-lines    deleted-lines  file-path1
  #  added-lines    deleted-lines  file-path2
  #  ...
  repo_dir = git_dir or os.getcwd()
  command = [
      'git', '-C', repo_dir, 'log', '--numstat', '--no-renames',
      '--format=#:%al:%cs:%s', '--after=' + start_date, '--before=' + end_date,
      'chrome', 'components'
  ]
  try:
    proc = subprocess.Popen(
        command,
        bufsize=1,  # buffered mode
        stdout=subprocess.PIPE,
        universal_newlines=True)
  except subprocess.SubprocessError as e:
    print(f'{command} failed with code {e.returncode}.', file=sys.stderr)
    print(f'\nSTDERR: {e.stderr}', file=sys.stderr)
    print(f'\nSTDOUT: {e.stdout}', file=sys.stderr)
    raise

  author_stat_m12n = defaultdict(int)
  author_stat_legacy = defaultdict(int)
  total_m12n = 0
  total_legacy = 0
  prev_msg_len = 0
  revert_cl = False
  for raw_line in proc.stdout:
    if raw_line.isspace():
      continue
    line = raw_line.strip()
    if line.startswith('#'):  # patch summary line
      _, author, commit_date, *subject = line.split(':', 4)
      revert_cl = (subject[0].startswith('Revert')
                   or subject[0].startswith('Reland'))
    else:
      if revert_cl or not line.endswith('.java'):
        continue

      # Do not take into account the number of deleted lines, which can
      # turn the overall changes to negative. If a class was renamed,
      # for instance, what's deleted is added somewhere else, so counting
      # only for addition works. Other kinds of deletion will be ignored.
      added, _deleted, path = line.split()
      diff = int(added)
      if _is_m12n_path(path):
        total_m12n += diff
        author_stat_m12n[author] += diff
      elif _is_legacy_path(path):
        total_legacy += diff
        author_stat_legacy[author] += diff

    msg = f'\rProcessing {commit_date} by {author}'
    if not quiet: _print_progress(msg, prev_msg_len)
    prev_msg_len = len(msg)

  if not quiet:
    _print_progress('Processing complete', prev_msg_len)
    print('\n')

  rankings_modularized = OrderedDict(
      sorted(author_stat_m12n.items(), key=lambda x: x[1], reverse=True))
  rankings_legacy = OrderedDict(
      sorted(author_stat_legacy.items(), key=lambda x: x[1], reverse=True))

  if json_format:
    return json.dumps({
        KEY_LOC_MODULARIZED: total_m12n,
        KEY_LOC_LEGACY: total_legacy,
        KEY_RANKINGS_MODULARIZED: rankings_modularized,
        KEY_RANKINGS_LEGACY: rankings_legacy,
        KEY_START_DATE: start_date,
        KEY_END_DATE: end_date,
    })
  else:
    output = []
    total = total_m12n + total_legacy
    percentage = 100.0 * total_m12n / total if total > 0 else 0
    output.append(f'# of lines added in modularized files: {total_m12n}')
    output.append(f'# of lines added in non-modularized files: {total_legacy}')
    output.append(f'% of lines landing in modularized files: {percentage:2.2f}')

    # Shows the top 50 contributors in each category.
    output.extend(
        _print_ranking(rankings_modularized, total_m12n,
                       'modules and components'))
    output.extend(
        _print_ranking(rankings_legacy, total_legacy, 'legacy and glue'))

    return '\n'.join(output)


def _print_ranking(rankings: OrderedDict, total: int, label: str) -> List[str]:
  if not rankings:
    return []

  output = []
  output.append(f'\nTop contributors ({label}):')
  output.append('No  lines    %    author')
  for rank, author in enumerate(list(rankings.keys())[:50], 1):
    lines = rankings[author]
    if lines == 0:
      break
    ratio = 100 * lines / total
    output.append(f'{rank:2d} {lines:6d} {ratio:5.1f}  {author}')
  return output


def _is_m12n_path(path):
  for prefix in _M12N_DIRS:
    if path.startswith(prefix):
      return True
  return False


def _is_legacy_path(path):
  return path.startswith(_LEGACY_DIR)


def _print_progress(msg, prev_msg_len):
  msg_len = len(msg)

  # Add spaces to remove the previous progress output completely.
  if msg_len < prev_msg_len:
    msg += ' ' * (prev_msg_len - msg_len)
  print(msg, end='\r')


def GetDateRange(*, past_days: int) -> Tuple[str, str]:
  """Returns start and end date for a period of past days.

  Use the results as start_date and end_date of GenerateLOCStats.
  """
  today = datetime.date.today()
  delta = datetime.timedelta(days=past_days)
  past = datetime.datetime(today.year, today.month, today.day) - delta
  return (past.date().isoformat(), today.isoformat())


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Generates LOC stats for modularization effort.")

  date_group = parser.add_mutually_exclusive_group(required=True)
  date_group.add_argument('--date',
                          type=str,
                          metavar=('<date-from>', '<date-to>'),
                          nargs=2,
                          help='date range (YYYY-MM-DD)~(YYYY-MM-DD)')
  date_group.add_argument('--past-days',
                          type=int,
                          help='The number of days to look back for stats. '
                          '0 for today only.')
  parser.add_argument('-q',
                      '--quiet',
                      action='store_true',
                      help='Do not output any message while processing')
  parser.add_argument('-j',
                      '--json',
                      action='store_true',
                      help='Output result in json format. '
                      'If not specified, output in more human-readable table.')
  parser.add_argument('-o',
                      '--output',
                      type=str,
                      help='File to write the result to in json format. '
                      'If not specified, outputs to console.')
  parser.add_argument('--git-dir',
                      type=str,
                      help='Root directory of the git repo to look into. '
                      'If not specified, use the current directory.')
  args = parser.parse_args()
  if args.past_days and args.past_days < 0:
    raise parser.error('--past-days must be non-negative.')

  if args.date:
    start_date, end_date = args.date
  else:
    start_date, end_date = GetDateRange(past_days=args.past_days)

  result = GenerateLOCStats(start_date,
                            end_date,
                            quiet=args.quiet,
                            json_format=args.json,
                            git_dir=args.git_dir)
  if args.output:
    with open(args.output, 'w') as f:
      f.write(result)
  else:
    print(result)
