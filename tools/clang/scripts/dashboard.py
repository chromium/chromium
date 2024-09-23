#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_REPO = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_REPO = ''  # This gets filled in by main().
RUST_REPO = ''  # This gets filled in by main().

# This script produces the dashboard at
# https://commondatastorage.googleapis.com/chromium-browser-clang/toolchain-dashboard.html
#
# Usage:
#
# ./dashboard.py > /tmp/toolchain-dashboard.html
# gsutil.py cp -a public-read /tmp/toolchain-dashboard.html gs://chromium-browser-clang/

#TODO: Add libc++ graph.
#TODO: Plot 30-day moving averages.
#TODO: Overview with current age of each toolchain component.
#TODO: Tables of last N rolls for each component.
#TODO: Link to next roll bug, count of blockers, etc.


def timestamp_to_str(timestamp):
  '''Return a string representation of a Unix timestamp.'''
  return time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(timestamp))


def get_git_timestamp(repo, revision):
  '''Get the Unix timestamp of a git commit.'''
  out = subprocess.check_output([
      'git', '-C', repo, 'show', '--date=unix', '--pretty=fuller', '--no-patch',
      revision
  ]).decode('utf-8')
  DATE_RE = re.compile(r'^CommitDate: (\d+)$', re.MULTILINE)
  m = DATE_RE.search(out)
  return int(m.group(1))


svn2git_dict = None


def svn2git(svn_rev):
  global svn2git_dict
  if not svn2git_dict:
    # The JSON was generated with:
    # $ ( echo '{' && git rev-list 40c47680eb2a1cb9bb7f8598c319335731bd5204 | while read commit ; do SVNREV=$(git log --format=%B -n 1 $commit | grep '^llvm-svn: [0-9]*$' | awk '{print $2 }') ; [[ ! -z '$SVNREV' ]] && echo "\"$SVNREV\": \"$commit\"," ; done && echo '}' ) | tee /tmp/llvm_svn2git.json
    # and manually removing the trailing comma of the last entry.
    with urllib.request.urlopen(
        'https://commondatastorage.googleapis.com/chromium-browser-clang/llvm_svn2git.json'
    ) as url:
      svn2git_dict = json.load(url)
    # For branch commits, use the most recent commit to main instead.
    svn2git_dict['324578'] = '93505707b6d3ec117e555c5a48adc2cc56470e38'
    svn2git_dict['149886'] = '60fc2425457f43f38edf5b310551f996f4f42df8'
    svn2git_dict['145240'] = '12330650f843cf7613444e345a4ecfcf06923761'
  return svn2git_dict[svn_rev]


def clang_rolls():
  '''Return a dict from timestamp to clang revision rolled in at that time.'''
  FIRST_ROLL = 'd78457ce2895e5b98102412983a979f1896eca90'
  log = subprocess.check_output([
      'git', '-C', CHROMIUM_REPO, 'log', '--date=unix', '--pretty=fuller', '-p',
      f'{FIRST_ROLL}..origin/main', '--', 'tools/clang/scripts/update.py',
      'tools/clang/scripts/update.sh'
  ]).decode('utf-8')

  # AuthorDate is when a commit was first authored; CommitDate (part of
  # --pretty=fuller) is when a commit was last updated. We use the latter since
  # it's more likely to reflect when the commit become part of upstream.
  DATE_RE = re.compile(r'^CommitDate: (\d+)$')
  VERSION_RE = re.compile(
      r'^\+CLANG_REVISION = \'llvmorg-\d+-init-\d+-g([0-9a-f]+)\'$')
  VERSION_RE_OLD = re.compile(r'^\+CLANG_REVISION = \'([0-9a-f]{10,})\'$')
  # +CLANG_REVISION=125186
  VERSION_RE_SVN = re.compile(r'^\+CLANG_REVISION ?= ?\'?(\d{1,6})\'?$')

  rolls = {}
  date = None
  for line in log.splitlines():
    m = DATE_RE.match(line)
    if m:
      date = int(m.group(1))
      next

    rev = None
    if m := VERSION_RE.match(line):
      rev = m.group(1)
    elif m := VERSION_RE_OLD.match(line):
      rev = m.group(1)
    elif m := VERSION_RE_SVN.match(line):
      rev = svn2git(m.group(1))

    if rev:
      assert (date)
      rolls[date] = rev
      date = None

  return rolls


def roll_ages(rolls, upstream_repo):
  '''Given a dict from timestamps to upstream revisions, return a list of pairs
    of timestamp string and *upstream revision age* in days at that timestamp.'''

  ages = []
  def add(timestamp, rev):
    ages.append(
        (timestamp_to_str(timestamp),
         (timestamp - get_git_timestamp(upstream_repo, rev)) / (3600 * 24)))

  assert (rolls)
  prev_roll_rev = None
  for roll_time, roll_rev in sorted(rolls.items()):
    if prev_roll_rev:
      add(roll_time - 1, prev_roll_rev)
    add(roll_time, roll_rev)
    prev_roll_rev = roll_rev
  add(time.time(), prev_roll_rev)

  return ages


def rust_rolls():
  '''Return a dict from timestamp to Rust revision rolled in at that time.'''
  FIRST_ROLL = 'c77dda41d8904b6c03083cd939733d9f754b0aeb'
  # Some rolls used CIPD version numbers (dates) instead of Git hashes.
  CIPD_ROLLS = {
      '20220914': '63b8d9b6898ec926f9eafa372506b6722d583694',
      '20221101': 'b7d9af278cc7e2d3bc8845156a0ab405a3536724',
      '20221118': '9db23f8d30e8d00e2e5e18b51f7bb8e582520600',
      '20221207': 'a09e8c55c663d2b070f99ab0fdadbcc2c45656b2',
      '20221209': '9553a4d439ffcf239c12142a78aa9923058e8a78',
      '20230117': '925dc37313853f15dc21e42dc869b024fe488ef3',
  }
  log = subprocess.check_output([
      'git', '-C', CHROMIUM_REPO, 'log', '--date=unix', '--pretty=fuller', '-p',
      f'{FIRST_ROLL}..origin/main', '--', 'tools/rust/update_rust.py'
  ]).decode('utf-8')

  # AuthorDate is when a commit was first authored; CommitDate (part of
  # --pretty=fuller) is when a commit was last updated. We use the latter since
  # it's more likely to reflect when the commit become part of upstream.
  DATE_RE = re.compile(r'^CommitDate: (\d+)$')
  VERSION_RE = re.compile(r'^\+RUST_REVISION = \'([0-9a-f]+)\'$')

  rolls = {}
  date = None
  for line in log.splitlines():
    m = DATE_RE.match(line)
    if m:
      date = int(m.group(1))
      next

    rev = None
    if m := VERSION_RE.match(line):
      rev = m.group(1)
      if rev in CIPD_ROLLS:
        rev = CIPD_ROLLS[rev]

    if rev:
      assert (date)
      rolls[date] = rev
      date = None

  return rolls


def print_dashboard():
  print('''
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Chromium Toolchain Dashboard</title>
    <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
    <script type="text/javascript">
      google.charts.load('current', {'packages':['corechart', 'controls']});
      google.charts.setOnLoadCallback(drawCharts);

      function drawCharts() {
        drawClangChart();
        drawRustChart();
      }

      function drawClangChart() {
        var data = google.visualization.arrayToDataTable([
['Date', 'Clang'],''')

  clang_ages = roll_ages(clang_rolls(), LLVM_REPO)
  for time_str, age in clang_ages:
    print(f'[new Date("{time_str}"), {age:.1f}],')

  print(''']);
        var dashboard = new google.visualization.Dashboard(document.getElementById('clang_dashboard'));
        var filter = new google.visualization.ControlWrapper({
          controlType: 'ChartRangeFilter',
          containerId: 'clang_filter',
          options: {
            filterColumnIndex: 0,
            ui: { chartOptions: { interpolateNulls: true, } },
          },
          state: {
            // Start showing roughly the last 6 months.
            range: { start: new Date(Date.now() - 1000 * 3600 * 24 * 31 * 6), },
          },
        });
        var chart = new google.visualization.ChartWrapper({
          chartType: 'LineChart',
          containerId: 'clang_chart',
          options: {
            width: 900,
            title: 'Chromium Toolchain Age Over Time',
            legend: 'top',
            vAxis: { title: 'Age (days)' },
            interpolateNulls: true,
            chartArea: {'width': '80%', 'height': '80%'},
          }
        });
        dashboard.bind(filter, chart);
        dashboard.draw(data);
      }

      function drawRustChart() {
        var data = google.visualization.arrayToDataTable([
['Date', 'Rust'],''')

  rust_ages = roll_ages(rust_rolls(), RUST_REPO)
  for time_str, age in rust_ages:
    print(f'[new Date("{time_str}"), {age:.1f}],')

  print(''']);
        var dashboard = new google.visualization.Dashboard(document.getElementById('rust_dashboard'));
        var filter = new google.visualization.ControlWrapper({
          controlType: 'ChartRangeFilter',
          containerId: 'rust_filter',
          options: {
            filterColumnIndex: 0,
            ui: { chartOptions: { interpolateNulls: true, } },
          },
          state: {
            // Start showing roughly the last 6 months.
            range: { start: new Date(Date.now() - 1000 * 3600 * 24 * 31 * 6), },
          },
        });
        var chart = new google.visualization.ChartWrapper({
          chartType: 'LineChart',
          containerId: 'rust_chart',
          options: {
            width: 900,
            title: 'Chromium Toolchain Age Over Time',
            legend: 'top',
            vAxis: { title: 'Age (days)' },
            interpolateNulls: true,
            chartArea: {'width': '80%', 'height': '80%'},
          }
        });
        dashboard.bind(filter, chart);
        dashboard.draw(data);
      }

    </script>
  </head>
  <body>
    <h1>Chromium Toolchain Dashboard (go/chrome-clang-dash)</h1>''')

  print(f'<p>Last updated: {timestamp_to_str(time.time())} UTC</p>')

  print('''
    <h2 id="clang">Clang</h2>
    <div id="clang_dashboard">
      <div id="clang_chart" style="width: 900px; height: 500px"></div>
      <div id="clang_filter" style="width: 900px; height: 50px"></div>
    </div>

    <h2 id="rust">Rust</h2>
    <div id="rust_dashboard">
      <div id="rust_chart" style="width: 900px; height: 500px"></div>
      <div id="rust_filter" style="width: 900px; height: 50px"></div>
    </div>
  </body>
</html>
''')


def main():
  parser = argparse.ArgumentParser(
      description='Generate Chromium toolchain dashboard.')
  parser.add_argument('--llvm-dir',
                      help='LLVM repository directory.',
                      default=os.path.join(CHROMIUM_REPO, '..', '..',
                                           'llvm-project'))
  parser.add_argument('--rust-dir',
                      help='Rust repository directory.',
                      default=os.path.join(CHROMIUM_REPO, '..', '..', 'rust'))
  args = parser.parse_args()

  global LLVM_REPO
  LLVM_REPO = args.llvm_dir
  if not os.path.isdir(os.path.join(LLVM_REPO, '.git')):
    print(f'Invalid LLVM repository path: {LLVM_REPO}')
    return 1

  global RUST_REPO
  RUST_REPO = args.rust_dir
  if not os.path.isdir(os.path.join(RUST_REPO, '.git')):
    print(f'Invalid Rust repository path: {RUST_REPO}')
    return 1

  print_dashboard()
  return 0


if __name__ == '__main__':
  sys.exit(main())
