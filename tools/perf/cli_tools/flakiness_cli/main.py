# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This tool provides a command line interface for the flakiness dashboard."""

from __future__ import print_function

import argparse

from cli_tools.flakiness_cli import analysis
from cli_tools.flakiness_cli import cached_api


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--master', help='include results for this master only, can use'
      ' shell-style wildcards to match multiple masters.')
  parser.add_argument(
      '--builder', help='include results for this builder only, can use'
      ' shell-style wildcards to match multiple builders.')
  parser.add_argument(
      '--test-type', help='include results for this test type only, can use'
      ' shell-style wildcards to match multiple test types.')
  parser.add_argument(
      '--test-suite', help='include results for this test suite only, can use'
      ' shell-style wildcards to match multiple test types.')
  parser.add_argument(
      '--half-life', default=7, type=int, help='test failures this many days'
      ' ago are half as important as failures today.')
  parser.add_argument(
      '--threshold', default=5.0, type=float, help='only show test '
      ' with flakiness above this level.')
  args = parser.parse_args()

  configs = cached_api.GetBuilders()
  configs = analysis.FilterBy(configs, master=args.master,
                              builder=args.builder, test_type=args.test_type)
  if configs.empty:
    return 'Your query selected no test configurations'

  dfs = []
  for row in configs.itertuples():
    df = cached_api.GetTestResults(row.master, row.builder, row.test_type)
    df = analysis.FilterBy(df, test_suite=args.test_suite)
    if df.empty:
      continue
    df = analysis.AggregateBuilds(df, args.half_life)
    df = df[df['flakiness'] > args.threshold]
    if df.empty:
      continue
    dfs.append(df)
  if not dfs:
    return 'Your query selected no test configurations'

  df = analysis.pandas.concat(dfs)
  df = df.sort_values('flakiness', ascending=False)
  print(df)
