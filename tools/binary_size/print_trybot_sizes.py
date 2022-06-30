#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints android-binary-size result for a given commit or commit range."""

import argparse
import collections
import concurrent.futures
import csv
import json
import os
import posixpath
import re
import subprocess
import sys

_COMMIT_LIMIT = 200
_LOG_RE = re.compile(
    r'^commit (\S+).*?'
    r'^Date:\s+(.*?)$.*?'
    r'^    (\S.*?)$.*?'
    r'^    Reviewed-on: (\S+).*?'
    r'^(?!commit)    Cr-Commit-Position:.*?(\d+)', re.DOTALL | re.MULTILINE)


_CommitInfo = collections.namedtuple(
    '_CommitInfo', 'git_hash date subject review_url cr_position')


def _git_log(git_log_args):
  cmd = ['git', 'log']

  # Ensure there's a limit on number of commits.
  if not any(x.startswith('-n') for x in git_log_args):
    cmd += [f'-n{_COMMIT_LIMIT}']

  cmd += git_log_args

  log_output = subprocess.check_output(cmd, encoding='utf8')
  ret = [_CommitInfo(*x) for x in _LOG_RE.findall(log_output)]

  if len(ret) == _COMMIT_LIMIT:
    sys.stderr.write(
        f'Limiting to {_COMMIT_LIMIT} commits. Use -n## to override\n')
  return ret


def _query_size(review_url):
  cmd = ['bb', 'ls', '-json', '-p']
  change_num = posixpath.basename(review_url)
  # Request results for all patchsets, assuming fewer than 30.
  for patchset in range(1, 30):
    cmd += [
        '-predicate',
        """{
"builder":{"project":"chromium","bucket":"try","builder":"android-binary-size"},
"gerrit_changes":[{
    "host":"chromium-review.googlesource.com","project":"chromium/src",
    "change":"%s","patchset":"%d"}
]}""" % (change_num, patchset)
    ]
  result = subprocess.run(cmd,
                          check=False,
                          stdout=subprocess.PIPE,
                          encoding='utf8')
  if result.returncode:
    return '<missing>'

  # Take the last one that has a size set.
  for json_str in result.stdout.splitlines()[::-1]:
    try:
      obj = json.loads(json_str)
    except json.JSONDecodeError:
      sys.stderr.write(f'Problem JSON:\n{json_str}\n')
      sys.exit(1)

    properties = obj.get('output', {}).get('properties', {})
    listings = properties.get('binary_size_plugin', {}).get('listings', [])
    for listing in listings:
      if listing['name'] == 'Android Binary Size':
        return listing['delta']
  return '<unknown>'


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--csv', action='store_true', help='Print as CSV')
  args, git_log_args = parser.parse_known_args()

  # Ensure user has authenticated.
  result = subprocess.run(['bb', 'auth-info'],
                          check=False,
                          stdout=subprocess.DEVNULL)
  if result.returncode:
    sys.stderr.write('First run: bb auth-login\n')
    sys.exit(1)

  commit_infos = _git_log(git_log_args)
  if not commit_infos:
    sys.stderr.write('Did not find any commits.\n')
    sys.exit(1)

  print(f'Fetching bot results for {len(commit_infos)} commits...')

  if args.csv:
    print_func = csv.writer(sys.stdout).writerow
  else:
    print_func = lambda v: print('{:12}{:14}{:12}{:32}{}'.format(*v))

  print_func(('Commit #', 'Git Hash', 'Size', 'Date', 'Subject'))
  with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:
    sizes = [pool.submit(_query_size, info.review_url) for info in commit_infos]
    for info, size in zip(commit_infos, sizes):
      size_str = size.result().replace(' bytes', '').lstrip('+')
      print_func((info.cr_position, info.git_hash[:12], size_str, info.date,
                  info.subject))


if __name__ == '__main__':
  main()
