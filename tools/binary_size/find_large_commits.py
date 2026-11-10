#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the large commits for a target from the perf dashboard.

Sizes come from the android-builder-perf bot via the Skia Perf dashboard
(perf.luci.app). Each commit also lists the arm32 delta reported by the
android-internal-binary-size trybot, or by android-binary-size when the
internal one has no result (requires "bb auth-login").

Skia Perf stores values as float32, so sizes are accurate to only ~8
significant digits.

Example:
  tools/binary_size/find_large_commits.py --since-crrev=1700000 \\
      --target=ChromeAndWebViewGoogle
"""

import argparse
import concurrent.futures
import json
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

import print_trybot_sizes


# Commit ranges where perf bot was giving invalid results.
# Range objects implement __contains__ for fast "in" operators.
_BAD_COMMIT_RANGES = [
  range(1045024, 1045552),  # https://crbug.com/1361952
]

_PERF_HOST = 'https://perf.luci.app'
# Skia Perf's vec32.MissingDataSentinel.
_MISSING = 1e32
# Responses include per-commit metadata (~15MB per 90 days), so larger
# requests can time out.
_CHUNK_DAYS = 30

# Maps --target to (resource_sizes benchmark names, metric). Benchmarks that
# were renamed list every name they have had.
_TARGETS = {
  'ChromeAndWebViewGoogle': (
    ['TrichromeGoogle', 'ChromeAndWebViewGoogle'],
    'Combined_normalized apk size',
  ),
  'SystemWebViewGoogle.minimal.apks': (
    ['SystemWebViewGoogle.minimal.apks'],
    'normalized apk size',
  ),
}


def _FetchJson(path, data=None):
  if data is not None:
    data = json.dumps(data).encode()
  req = urllib.request.Request(
    _PERF_HOST + path, data=data, headers={'Content-Type': 'application/json'}
  )
  with urllib.request.urlopen(req, timeout=300) as resp:
    return json.load(resp)


def _CrrevTimestamp(crrev):
  commits = _FetchJson('/_/cid', [crrev])['commitSlice']
  if not commits:
    sys.exit(f'Unknown crrev: {crrev}')
  return commits[0]['ts']


def _QueryChunk(query, begin, end):
  """Returns {rev: size} for all traces matching |query| in [begin, end]."""
  req = {
    'begin': begin,
    'end': end,
    'queries': [query],
    'tz': 'UTC',
    'request_type': 0,  # REQUEST_TIME_RANGE
  }
  # The server intermittently fails queries under load. Back off 15s, 30s, ...
  for attempt in range(5):
    if attempt:
      time.sleep(15 * attempt)
    try:
      # The server fails jobs that exceed its own deadline, so this loop ends.
      status = _FetchJson('/_/frame/start', req)
      while status['status'] == 'Running':
        time.sleep(1)
        status = _FetchJson(status['url'])
    except (urllib.error.URLError, TimeoutError) as e:
      sys.stderr.write(f'Attempt {attempt + 1} failed: {e}\n')
      continue
    if status['status'] == 'Finished':
      break
    sys.stderr.write(f'Attempt {attempt + 1} failed: {status["messages"]}\n')
  else:
    sys.exit('Giving up.')

  df = status['results']['dataframe']
  ret = {}
  # Renamed benchmarks are separate traces that do not overlap.
  for values in df['traceset'].values():
    for col, value in zip(df['header'], values):
      if value is not None and value < _MISSING:
        ret[col['offset']] = int(value)
  return ret


def _FetchRevsAndSizes(target, since_crrev):
  """Returns a sorted list of (rev, size) from the perf dashboard."""
  benchmarks, metric = _TARGETS[target]
  params = {
    'master': 'ChromiumPerf',
    'bot': 'android-builder-perf',
    'benchmark': [f'resource_sizes ({b})' for b in benchmarks],
    'test': 'Specifics',
    'subtest_1': metric,
    'stat': 'value',
  }
  query = urllib.parse.urlencode(
    params, doseq=True, quote_via=urllib.parse.quote
  )

  sizes = {}
  end = int(time.time())
  # Commit timestamps are not strictly increasing, so fetch an extra day and
  # filter by revision.
  begin_all = _CrrevTimestamp(since_crrev) - 86400
  while end > begin_all:
    begin = max(begin_all, end - _CHUNK_DAYS * 86400)
    sys.stderr.write(
      'Fetching sizes for {} -> {}\n'.format(
        time.strftime('%Y-%m-%d', time.gmtime(begin)),
        time.strftime('%Y-%m-%d', time.gmtime(end)),
      )
    )
    sizes.update(_QueryChunk(query, begin, end))
    end = begin
    time.sleep(1)  # Be gentle with the server.
  return sorted((r, s) for r, s in sizes.items() if r >= since_crrev)


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
    ['git', 'crrev-parse', str(rev)], encoding="utf-8"
  ).strip()
  if not sha1:
    raise Exception(
      f'git crrev-parse for {rev} failed. Probably need to '
      f'"git fetch origin main"'
    )
  desc = subprocess.check_output(['git', 'log', '-n1', sha1], encoding="utf-8")
  author = re.search(r'Author: .*?<(.*?)>', desc).group(1)
  day, year = re.search(r'Date:\s+\w+\s+(\w+ \d+)\s+.*?\s+(\d+)', desc).groups()
  date = '{} {}'.format(day, year)
  title = re.search(r'\n +(\S.*)', desc).group(1).replace('\t', ' ')
  review_url = print_trybot_sizes.extract_review_url(desc)
  return sha1, author, date, title, review_url


def _QueryTrybotSize(review_url):
  """Returns the internal trybot's arm32 delta, else the public trybot's."""
  size = print_trybot_sizes.query_size(review_url, internal=True)[0]
  # Placeholders like "<unknown>" mean there is no result.
  if size.startswith('<'):
    size = print_trybot_sizes.query_size(review_url, internal=False)[0]
  return size


def main():
  parser = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawTextHelpFormatter
  )
  parser.add_argument(
    '--increase-threshold',
    type=int,
    default=30 * 1024,
    help='Minimum number of bytes larger to be considered a notable.',
  )
  parser.add_argument(
    '--decrease-threshold',
    type=int,
    default=30 * 1024,
    help='Minimum number of bytes smaller to be considered a notable.',
  )
  parser.add_argument(
    '--since-crrev',
    type=int,
    required=True,
    help='Earliest commit position to include.',
  )
  parser.add_argument('--target', required=True, choices=sorted(_TARGETS))
  options = parser.parse_args()

  print_trybot_sizes.check_bb_auth()

  # git crrev-parse (below) needs origin/main to contain the newest perf
  # revisions. Chromium lands commits every few minutes.
  origin_main_time = int(
    subprocess.check_output(
      ['git', 'log', '-n1', '--format=%ct', 'origin/main'], encoding='utf-8'
    )
  )
  age_minutes = (time.time() - origin_main_time) / 60
  if age_minutes > 10:
    sys.stderr.write(
      f'origin/main is {age_minutes:.0f} minutes old. Fetching.\n'
    )
    subprocess.check_call(['git', 'fetch', 'origin', 'main'])

  revs_and_sizes = _FetchRevsAndSizes(options.target, options.since_crrev)
  if not revs_and_sizes:
    sys.exit('No sizes found.')
  big_deltas = _FindBigDeltas(
    revs_and_sizes, options.increase_threshold, options.decrease_threshold
  )

  print(
    'Printing info for up to {} commits in the range {}-{}'.format(
      len(big_deltas), revs_and_sizes[0][0], revs_and_sizes[-1][0]
    )
  )
  print('Revision,Hash,Title,Author,Delta,Trybot Delta,Date')
  num_bad_commits = 0
  with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:
    rows = []
    for rev, delta, prev_rev in big_deltas:
      if any(rev in r for r in _BAD_COMMIT_RANGES):
        num_bad_commits += 1
        continue
      sha1, author, date, title, review_url = _LookupCommitInfo(rev)
      rev_str = str(rev)
      if rev - prev_rev > 1:
        rev_str = f'{prev_rev}..{rev}'
      trybot_size = pool.submit(_QueryTrybotSize, review_url)
      rows.append(
        ([rev_str, sha1, title, author, str(delta)], trybot_size, date)
      )

    for cols, trybot_size, date in rows:
      print('\t'.join([*cols, trybot_size.result(), date]))

  if num_bad_commits:
    print(f'Ignored {num_bad_commits} commits from bad ranges')


if __name__ == '__main__':
  main()
