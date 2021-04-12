# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import os
import sys
import shutil
import subprocess
import tempfile

CHROMIUM_PATH = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
TOOLS_PERF_PATH = os.path.join(CHROMIUM_PATH, 'tools', 'perf')
sys.path.insert(1, TOOLS_PERF_PATH)

from core.external_modules import pandas

RUNS_USED_FOR_LIMIT_UPDATE = 50
CHANGE_PERCENTAGE_LIMIT = 0.01

SWARMING_PATH = os.path.join(CHROMIUM_PATH, 'tools', 'luci-go', 'swarming')
UPPER_LIMITS_DATA_DIR = os.path.join(
  CHROMIUM_PATH, 'testing', 'scripts', 'representative_perf_test_data')


def FetchItemIds(tags, limit):
  """Fetches the item id of tasks described by the tags.

  Args:
    tags: The tags which describe the task such as OS, builder_group and buildername.
    limit: The number of runs to look at.

  Returns:
    A list containing the item Id of the tasks.
  """

  query = [
      SWARMING_PATH, 'tasks', '-S', 'chromium-swarm.appspot.com', '-limit',
      str(limit), '-state=COMPLETED', '-field', 'items(task_id)', '-tag',
      'builder_group:{builder_group}'.format(**tags), '-tag',
      'os:{os}'.format(**tags), '-tag',
      'name:rendering_representative_perf_tests', '-tag',
      'buildername:{buildername}'.format(**tags)
  ]
  return json.loads(subprocess.check_output(query))


def FetchItemData(task_id, benchmark, index, temp_dir):
  """Fetches the performance values (AVG & CI ranges) of tasks.

  Args:
    task_id: The list of item Ids to fetch dat for.
    benchmark: The benchmark these task are on (desktop/mobile).
    index: The index field of the data_frame
    temp_dir: The temp directory to store task data in.

  Returns:
    A data_frame containing the averages and confidence interval ranges.
  """
  query = [
      SWARMING_PATH, 'collect', '-S', 'chromium-swarm.appspot.com',
      '-output-dir', temp_dir, '-perf', task_id
  ]
  try:
    subprocess.check_output(query)
  except Exception as e:
    print(e)

  result_file_path = os.path.join(temp_dir, task_id, 'rendering.' + benchmark,
                                  'perf_results.csv')

  try:
    df = pandas.read_csv(result_file_path)
    df_frame_times = df.loc[df['name'] == 'frame_times']
    df_frame_times = df_frame_times[['stories', 'avg', 'ci_095']]

    df_cpu_wall = df.loc[df['name'] == 'cpu_wall_time_ratio']
    df_cpu_wall = df_cpu_wall[['stories', 'avg']]
    df_cpu_wall = df_cpu_wall.rename(columns={'avg': 'cpu_wall_time_ratio'})

    df = pandas.merge(df_frame_times, df_cpu_wall, on='stories')
    df['index'] = index
    return df
  except:
    print("CSV results were not produced!")


def CreateDataframe(benchmark, tags, limit):
  """Creates the dataframe of values recorded in recent runs.

  Given the tags, benchmark this function fetches the data of last {limit}
  runs, and returns a dataframe of values for focused metrics such as
  frame_times and CPU_wall_time_ratio.

  Args:
    benchmark: The benchmark these task are on (desktop/mobile).
    tags: The tags which describe the tasks such as OS and buildername.
    limit: The number of runs to look at.

  Returns:
    A dataframe with averages and confidence interval of frame_times, and
    average value of CPU_wall_time_ratio of each story of each run.
  """
  items = []
  for tag_set in tags:
    items.extend(FetchItemIds(tag_set, limit))

  dfs = []
  try:
    temp_dir = tempfile.mkdtemp('perf_csvs')
    for idx, item in enumerate(items):
      dfs.append(FetchItemData(item['task_id'], benchmark, idx, temp_dir))
      idx += 1
  finally:
    shutil.rmtree(temp_dir)
  return pandas.concat(dfs, ignore_index=True)


def GetPercentileValues(data_frame, percentile):
  """Get the percentile value of each metric for recorded values in dataframe.

  Args:
    data_frame: The dataframe with averages and confidence intervals of each
    story of each run.
    percentile: the percentile to use for determining the upper limits.

  Returns:
    A dictionary with averages and confidence interval ranges calculated
    from the percentile of recent runs.
  """

  if not data_frame.empty:
    avg_df = data_frame.pivot(index='stories', columns='index', values='avg')
    upper_limit = avg_df.quantile(percentile, axis = 1)
    ci_df = data_frame.pivot(index='stories', columns='index', values='ci_095')
    upper_limit_ci = ci_df.quantile(percentile, axis = 1)
    cpu_wall_df = data_frame.pivot(index='stories',
                                   columns='index',
                                   values='cpu_wall_time_ratio')
    upper_limit_cpu_wall = cpu_wall_df.quantile(1 - percentile, axis=1)

    results = {}
    for index in avg_df.index:
      results[index] = {
          'avg': round(upper_limit[index], 3),
          'ci_095': round(upper_limit_ci[index], 3),
          'cpu_wall_time_ratio': round(upper_limit_cpu_wall[index], 3)
      }
    return results


def MeasureNewUpperLimit(old_value, new_value, att_name, max_change):
  change_pct = 0.0
  if old_value > 0:
    change_pct = (new_value - old_value) / old_value

  print(
    '  {}:\t\t {} -> {} \t({:.2f}%)'.format(
      att_name, old_value, new_value, change_pct * 100))
  if new_value < 0.01:
    print('WARNING: New selected value is close to 0.')
  return (
      round(new_value, 3),
      max(max_change, abs(change_pct))
  )


def RecalculateUpperLimits(data_point_count):
  """Recalculates the upper limits using the data of recent runs.

  This method replaces the existing JSON file which contains the upper limits
  used by representative perf tests if the changes of upper limits are
  significant.

  Args:
    data_point_count: The number of runs to use for recalculation.
  """
  with open(os.path.join(UPPER_LIMITS_DATA_DIR,
            'platform_specific_tags.json')) as tags_data:
    platform_specific_tags = json.load(tags_data)

  with open(
    os.path.join(
      UPPER_LIMITS_DATA_DIR,
      'representatives_frame_times_upper_limit.json')) as current_data:
    current_upper_limits = json.load(current_data)

  max_change = 0.0
  results = {}
  for platform in platform_specific_tags:
    platform_data = platform_specific_tags[platform]
    print('\n- Processing data ({})'.format(platform))

    dataframe = CreateDataframe(platform_data['benchmark'],
                                platform_data['tags'], data_point_count)
    results[platform] = GetPercentileValues(dataframe, 0.95)

    # Loop over results and adjust base on current values.
    for story in results[platform]:
      if story in current_upper_limits[platform]:
        print(story, ':')
        new_avg, max_change = MeasureNewUpperLimit(
          current_upper_limits[platform][story]['avg'],
          results[platform][story]['avg'], 'AVG', max_change)
        results[platform][story]['avg'] = new_avg

        new_ci, max_change = MeasureNewUpperLimit(
          current_upper_limits[platform][story]['ci_095'],
          results[platform][story]['ci_095'], 'CI', max_change)
        results[platform][story]['ci_095'] = new_ci

        new_cpu_ratio, max_change = MeasureNewUpperLimit(
            current_upper_limits[platform][story]['cpu_wall_time_ratio'],
            results[platform][story]['cpu_wall_time_ratio'],
            'CPU_wall_time_ratio', max_change)
        results[platform][story]['cpu_wall_time_ratio'] = new_cpu_ratio

        if current_upper_limits[platform][story].get('control', False):
          results[platform][story]['control'] = True
        if current_upper_limits[platform][story].get('experimental', False):
          results[platform][story]['experimental'] = True
        comment = current_upper_limits[platform][story].get('_comment', False)
        if not comment == False:
          results[platform][story]['_comment'] = comment

  if max_change > CHANGE_PERCENTAGE_LIMIT:
    with open(
      os.path.join(
        UPPER_LIMITS_DATA_DIR,
        'representatives_frame_times_upper_limit.json'
      ), 'w') as outfile:
      json.dump(results, outfile, separators=(',', ': '), indent=2)
    print(
      'Upper limits were updated on '
      'representatives_frame_times_upper_limit.json')
  else:
    print('Changes are small, no need for new limits')


if __name__ == '__main__':
  sys.exit(RecalculateUpperLimits(RUNS_USED_FOR_LIMIT_UPDATE))
