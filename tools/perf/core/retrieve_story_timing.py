#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import json
import subprocess
import sys


_CLOUD_PROJECT_ID = 'test-results-hrd'
QUERY_BY_BUILD_NUMBER = """
SELECT
  run.name AS name,
  SUM(run.times) AS duration
FROM
  [%s:events.test_results]
WHERE
  buildbot_info.builder_name IN ({})
  AND buildbot_info.build_number = {}
GROUP BY
  name
ORDER BY
  name
""" % _CLOUD_PROJECT_ID
QUERY_STORY_AVG_RUNTIME = """
SELECT
  name,
  ROUND(AVG(time)) AS duration,
FROM (
  SELECT
    run.name AS name,
    start_time,
    AVG(run.times) AS time
  FROM
    [%s:events.test_results]
  WHERE
    buildbot_info.builder_name IN ({configuration_names})
    AND run.time IS NOT NULL
    AND run.time != 0
    AND run.is_unexpected IS FALSE
    AND DATEDIFF(CURRENT_DATE(), DATE(start_time)) < {num_last_days}
    AND _PARTITIONTIME > DATE_ADD(CURRENT_DATE(), -{num_last_days}, "DAY")
  GROUP BY
    name,
    start_time
  ORDER BY
    start_time DESC)
GROUP BY
  name
ORDER BY
  name
""" % _CLOUD_PROJECT_ID
QUERY_STORY_TOTAL_RUNTIME = """
SELECT
  name,
  ROUND(AVG(time)) AS duration,
FROM (
  SELECT
    run.name AS name,
    start_time,
    SUM(run.times) AS time
  FROM
    [%s:events.test_results]
  WHERE
    buildbot_info.builder_name IN ({configuration_names})
    AND run.time IS NOT NULL
    AND run.time != 0
    AND run.is_unexpected IS FALSE
    AND DATEDIFF(CURRENT_DATE(), DATE(start_time)) < {num_last_days}
    AND _PARTITIONTIME > DATE_ADD(CURRENT_DATE(), -{num_last_days}, "DAY")
  GROUP BY
    name,
    start_time
  ORDER BY
    start_time DESC)
GROUP BY
  name
ORDER BY
  name
""" % _CLOUD_PROJECT_ID


_BQ_SETUP_INSTRUCTION = """
** NOTE: this script is only for Googlers to use. **

bq script isn't found on your machine. To run this script, you need to be able
to run bigquery in your terminal.
If this is the first time you run the script, do the following steps:

1) Follow the steps at https://cloud.google.com/sdk/docs/ to download and
   unpack google-cloud-sdk in your home directory.
2) Run `gcloud auth login`
3) Run `gcloud config set project test-results-hrd`
   3a) If 'test-results-hrd' does not show up, contact chops-data@
       to be added as a user of the table
4) Run this script!
"""


def _run_query(query):
  try:
    subprocess.check_call(['which', 'bq'])
  except subprocess.CalledProcessError:
    raise RuntimeError(_BQ_SETUP_INSTRUCTION)
  args = ["bq", "query", "--project_id="+_CLOUD_PROJECT_ID, "--format=json",
          "--max_rows=100000", query]

  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  if p.returncode == 0:
    return json.loads(stdout)
  raise RuntimeError(
      'Error generating authentication token.\nStdout: %s\nStder:%s' %
      (stdout, stderr))


def FetchStoryTimingDataForSingleBuild(configurations, build_number):
  configurations_str = ','.join(repr(c) for c in configurations)
  return _run_query(QUERY_BY_BUILD_NUMBER.format(
      configurations_str, build_number))


def FetchAverageStoryTimingData(configurations, num_last_days):
  configurations_str = ','.join(repr(c) for c in configurations)
  return _run_query(QUERY_STORY_AVG_RUNTIME.format(
      configuration_names=configurations_str, num_last_days=num_last_days))


def FetchBenchmarkRuntime(configurations, num_last_days):
  test_total_runtime =  _run_query(QUERY_STORY_TOTAL_RUNTIME.format(
      configuration_names=configurations, num_last_days=num_last_days))
  benchmarks_data = collections.OrderedDict()
  total_runtime = 0
  total_num_stories = 0
  for item in test_total_runtime:
    duration = item['duration']
    test_name = item['name']
    benchmark_name, _ = test_name.split('/', 1)
    if not benchmark_name in benchmarks_data:
      benchmarks_data[benchmark_name] = {
          'num_stories': 0,
          'total_runtime_in_seconds': 0,
      }
    benchmarks_data[benchmark_name]['num_stories'] += 1
    total_num_stories += 1
    benchmarks_data[benchmark_name]['total_runtime_in_seconds'] += (
        float(duration))
    total_runtime += float(duration)
  benchmarks_data['All benchmarks'] = {
      'total_runtime_in_seconds': total_runtime,
      'num_stories': total_num_stories
  }
  return benchmarks_data


_FETCH_BENCHMARK_RUNTIME = 'fetch-benchmark-runtime'
_FETCH_STORY_RUNTIME = 'fetch-story-runtime'

def main(args):
  parser = argparse.ArgumentParser(
      description='Retrieve story timing from bigquery.')
  parser.add_argument('action',
      choices=[_FETCH_BENCHMARK_RUNTIME, _FETCH_STORY_RUNTIME])
  parser.add_argument(
      '--output-file', '-o', action='store', required=True,
      help='The filename to send the bigquery results to.')
  parser.add_argument(
      '--configurations', '-c', action='append', required=True,
      help='The configuration(s) of the builder to query results from.')
  parser.add_argument(
      '--build-number', action='store',
      help='If specified, the build number to get timing data from.')
  opts = parser.parse_args(args)


  if opts.action == _FETCH_BENCHMARK_RUNTIME:
    data = FetchBenchmarkRuntime(opts.configurations, num_last_days=5)
  else:
    if opts.build_number:
      data = FetchStoryTimingDataForSingleBuild(opts.configurations,
          opts.build_number)
    else:
      data = FetchAverageStoryTimingData(opts.configurations, num_last_days=5)

  with open(opts.output_file, 'w') as output_file:
    json.dump(data, output_file, indent = 4, separators=(',', ': '))

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
