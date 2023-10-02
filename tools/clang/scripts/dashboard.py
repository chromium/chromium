#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import subprocess
import sys
import time

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_REPO = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_REPO = ''  # This gets filled in by main().

# This script produces the dashboard at
# https://commondatastorage.googleapis.com/chromium-browser-clang/toolchain-dashboard.html
#
# Usage:
#
# ./dashboard.py > /tmp/toolchain-dashboard.html
# gsutil.py cp -a public-read /tmp/toolchain-dashboard.html gs://chromium-browser-clang/

#TODO: Add Rust and libc++ graphs.
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


def clang_rolls():
  '''Return a dict from timestamp to clang revision rolled in at that time.'''
  # The first roll using a git revision for the clang package version:
  FIRST_GIT_ROLL = '9549a986740e03ca362144559d7d8b69644f8c5e'
  log = subprocess.check_output([
      'git', '-C', CHROMIUM_REPO, 'log', '--date=unix', '--pretty=fuller', '-p',
      f'{FIRST_GIT_ROLL}..origin/main', '--', 'tools/clang/scripts/update.py'
  ]).decode('utf-8')

  # AuthorDate is when a commit was first authored; CommitDate (part of
  # --pretty=fuller) is when a commit was last updated. We use the latter since
  # it's more likely to reflect when the commit become part of upstream.
  DATE_RE = re.compile(r'^CommitDate: (\d+)$')
  VERSION_RE = re.compile(
      r'^\+CLANG_REVISION = \'llvmorg-\d+-init-\d+-g([0-9a-f]+)\'$')
  VERSION_RE_OLD = re.compile(r'^\+CLANG_REVISION = \'([0-9a-f]{10,})\'$')

  rolls = {}
  date = None
  for line in log.splitlines():
    m = DATE_RE.match(line)
    if m:
      date = int(m.group(1))
    m = VERSION_RE.match(line)
    if not m:
      m = VERSION_RE_OLD.match(line)
    if m:
      assert (date)
      rolls[date] = m.group(1)
      date = None
  return rolls


def clang_roll_ages(rolls):
  '''Given a dict from timestamps to clang revisions, return a list of pairs
    of timestamp string and clang revision age in days at that timestamp.'''

  ages = []
  def add(timestamp, rev):
    ages.append((timestamp_to_str(timestamp),
                 (timestamp - get_git_timestamp(LLVM_REPO, rev)) / (3600 * 24)))

  assert (rolls)
  prev_roll_rev = None
  for roll_time, roll_rev in sorted(rolls.items()):
    if prev_roll_rev:
      add(roll_time - 1, prev_roll_rev)
    add(roll_time, roll_rev)
    prev_roll_rev = roll_rev
  add(time.time(), prev_roll_rev)

  return ages


def print_dashboard():
  rolls = clang_rolls()
  ages = clang_roll_ages(rolls)

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
      google.charts.setOnLoadCallback(drawChart);

      function drawChart() {
        var data = google.visualization.arrayToDataTable([
['Date', 'Clang'],''')

  for time_str, age in ages:
    print(f'[new Date("{time_str}"), {age:.1f}],')

  print(''']);

        var dashboard = new google.visualization.Dashboard(document.getElementById('dashboard'));
        var filter = new google.visualization.ControlWrapper({
          controlType: 'ChartRangeFilter',
          containerId: 'filter',
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
          containerId: 'chart',
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
    <div id="dashboard">
      <div id="chart" style="width: 900px; height: 500px"></div>
      <div id="filter" style="width: 900px; height: 50px"></div>
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
  args = parser.parse_args()

  global LLVM_REPO
  LLVM_REPO = args.llvm_dir
  if not os.path.isdir(os.path.join(LLVM_REPO, '.git')):
    print(f'Invalid LLVM repository path: {LLVM_REPO}')
    return 1

  print_dashboard()
  return 0


if __name__ == '__main__':
  sys.exit(main())
