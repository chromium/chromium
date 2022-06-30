#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints android-binary-size result for a given commit or commit range."""

import argparse
import concurrent.futures
import json
import os
import posixpath
import re
import subprocess
import sys

_LOG_RE = re.compile(
    r'^commit (\S+).*?'
    r'^\s*Reviewed-on: (\S+).*?'
    r'^\s*Cr-Commit-Position:.*?(\d+)', re.DOTALL | re.MULTILINE)


def _git_log(rev_list):
  cmd = ['git', 'log', rev_list]
  if '..' not in rev_list:
    cmd += ['-n1']

  log_output = subprocess.check_output(cmd, encoding='utf8')
  for git_hash, review_url, cr_position in _LOG_RE.findall(log_output):
    yield git_hash, review_url, cr_position


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
  parser.add_argument('rev_list')
  args = parser.parse_args()

  # Ensure user has authenticated.
  result = subprocess.run(['bb', 'auth-info'],
                          check=False,
                          stdout=subprocess.DEVNULL)
  if result.returncode:
    print('First run: bb auth-login')
    sys.exit(1)

  commit_infos = list(_git_log(args.rev_list))
  with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:
    sizes = [pool.submit(_query_size, url) for _, url, _ in commit_infos]
    for (git_hash, _, cr_position), size in zip(commit_infos, sizes):
      print(f'{cr_position}\t{git_hash}\t{size.result()}')


if __name__ == '__main__':
  main()
