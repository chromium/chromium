# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile

TOOLS_PERF_PATH = os.path.abspath(os.path.join(
  os.path.dirname(__file__), '..', '..'))
sys.path.insert(1, TOOLS_PERF_PATH)

from experimental.story_clustering import similarity_calculator
from experimental.story_clustering import cluster_stories
from experimental.story_clustering import create_soundwave_input
from core.external_modules import pandas


def CalculateDistances(
  all_bots_dataframe,
  bots,
  rolling_window,
  metric_name,
  normalize = False):
  timeseries = []

  for bot_name, bot_group in all_bots_dataframe.groupby(bots):
    temp_dataframe = bot_group.pivot(index='test_case',
      columns='commit_pos', values='value')
    temp_dataframe_with_solling_avg = temp_dataframe.rolling(
      rolling_window,
      min_periods=1,
      axis=1
    ).mean().stack().rename('value').reset_index()

    temp_dataframe_with_solling_avg['bot'] = bot_name
    timeseries.append(temp_dataframe_with_solling_avg)

  all_bots = pandas.concat(timeseries)
  distance_matrix = similarity_calculator.CalculateDistances(
    all_bots,
    metric=metric_name,
    normalize=normalize,
  )
  print('Similarities are calculated for', metric_name)

  return distance_matrix


def Main(argv):
  parser = argparse.ArgumentParser(
    description=('Gathers the values of each metric and platfrom pair in a'
    ' csv file to be used in clustering of stories.'))
  parser.add_argument('benchmark', type=str, help='benchmark to be used')
  parser.add_argument('--metrics', type=str, nargs='*',
    help='List of metrics to use')
  parser.add_argument('--platforms', type=str, nargs='*',
    help='List of platforms to use')
  parser.add_argument('--testcases-path', type=str, help=('Path to the file '
    'containing a list of all test_cases in the benchmark that needs to '
    'be clustered'))
  parser.add_argument('--days', default=180, help=('Number of days to gather'
    ' data about'))
  parser.add_argument('--output-path', type=str, help='Output file',
    default='//tmp/story_clustering/clusters.json')
  parser.add_argument('--max-cluster-count', default='10',
    help='Number of not valid clusters needed')
  parser.add_argument('--min-cluster-size', default='2', help=('Least number '
            'of members in cluster, to make cluster valied'))
  parser.add_argument('--rolling-window', default='1', help=('Number of '
    'samples to take average from while calculating the moving average'))
  parser.add_argument('--normalize', default=False,
    help='Normalize timeseries to calculate similarity', action='store_true')
  args = parser.parse_args(argv[1:])

  temp_dir = tempfile.mkdtemp('telemetry')
  startup_timeseries = os.path.join(temp_dir, 'startup_timeseries.json')
  soundwave_output_path = os.path.join(temp_dir, 'data.csv')
  soundwave_path = os.path.join(TOOLS_PERF_PATH, 'soundwave')

  try:
    output_dir = os.path.dirname(args.output_path)
    clusters_json = {}

    if not os.path.isdir(output_dir):
      os.makedirs(output_dir)

    # creating the json file needed for soundwave
    create_soundwave_input.CreateInput(
      test_suite=args.benchmark,
      platforms=args.platforms,
      metrics=args.metrics,
      test_cases_path=args.testcases_path,
      output_dir=startup_timeseries)

    subprocess.call([
      soundwave_path,
      '-d', args.days,
      'timeseries',
      '-i', startup_timeseries,
      '--output-csv', soundwave_output_path
    ])

    # Processing the data.
    dataframe = pandas.read_csv(soundwave_output_path)
    dataframe_per_metric = dataframe.groupby(dataframe['measurement'])
    for metric_name, all_bots in list(dataframe_per_metric):
      clusters_json[metric_name] = []

      distance_matrix = CalculateDistances(
        all_bots_dataframe=all_bots,
        bots=dataframe['bot'],
        rolling_window=int(args.rolling_window),
        metric_name=metric_name,
        normalize=args.normalize)

      clusters, coverage = cluster_stories.RunHierarchicalClustering(
        distance_matrix,
        max_cluster_count=int(args.max_cluster_count),
        min_cluster_size=int(args.min_cluster_size),
      )
      print()
      print(metric_name, ':')
      print(format(coverage * 100.0, '.1f'), 'percent coverage.')
      print('Stories are grouped into', len(clusters), 'clusters.')
      print('representatives:')
      for cluster in clusters:
        print (cluster.GetRepresentative())
      print()

      for cluster in clusters:
        clusters_json[metric_name].append(cluster.AsDict())

    with open(args.output_path, 'w') as outfile:
      json.dump(
        clusters_json,
        outfile,
        separators=(',',': '),
        indent=4,
        sort_keys=True
      )

  except Exception:
    logging.exception('The following exception may have prevented the code'
      ' from clustering stories.')
  finally:
    shutil.rmtree(temp_dir, ignore_errors=True)

if __name__ == '__main__':
  sys.exit(Main(sys.argv))
