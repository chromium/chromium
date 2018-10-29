# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import logging
import os
import traceback

from benchmarks import loading

from telemetry import benchmark
from telemetry.internal import story_runner
from telemetry.value import none_values
from telemetry.value.list_of_scalar_values import StandardDeviation

def _ListSubtraction(diff_list, control_list):
  """Subtract |control_list|'s elements from the corresponding elements in
  |diff_list|, and store the results in |diff_list|.

  Lists may have different length and we will align with the shorter one.
  e.g. 'timeToInteractive' may be missing for some runs.
  """

  min_len = min(len(diff_list), len(control_list))
  for i in xrange(min_len):
    diff_list[i] = diff_list[i] - control_list[i]
  while len(diff_list) > min_len:
    diff_list.pop()

def _PointSubtraction(diff_point, control_point):
  """Subtract |control_point| from |diff_point| and store the result in
  |diff_point|.

  Args:
    diff_point: A chart point (could either be a story point (e.g. 'FIFA') or a
      'summary' point), will hold the result.
    control_point: A chart point.
  """

  if diff_point['type'] == 'scalar':
    diff_point['value'] = diff_point['value'] - control_point['value']
  elif diff_point['type'] == 'list_of_scalar_values':
    # Points may have None 'values' regardless their types.
    if not diff_point['values'] or not control_point['values']:
      none_value_reason = (
          none_values.MERGE_FAILURE_REASON +
          ' None values: %s' % repr([diff_point, control_point]))
      diff_point['values'] = None
      diff_point['std'] = None
      diff_point['none_value_reason'] = none_value_reason
      return
    _ListSubtraction(diff_point['values'], control_point['values'])
    diff_point['std'] = StandardDeviation(diff_point['values'])
  else:
    raise NotImplementedError('invalid point type: %s' % diff_point['type'])

def _RenameChartsAndPointsWithSuffix(charts, suffix):
  """Append |suffix| to all chart names (except 'trace') and point names
  (except 'summary').

  Args:
    charts: A dictionary of charts.
    suffix: A string suffix, e.g. '_control'.
  """

  # First rename all points except 'summary.
  for chart_name in charts:
    chart = charts[chart_name]
    old_point_names = chart.keys()
    for point_name in old_point_names:
      if point_name == 'summary':
        continue
      chart[point_name + suffix] = chart[point_name]
      chart.pop(point_name, None)

  # Then rename all charts except 'trace'.
  old_chart_names = charts.keys()
  for chart_name in old_chart_names:
    if chart_name == 'trace':
      continue
    chart = charts[chart_name]
    new_chart_name = chart_name + suffix
    for point_name in chart:
      chart[point_name]['name'] = new_chart_name
    charts[new_chart_name] = chart
    charts.pop(chart_name, None)

def _MergeCharts(dest_charts, source_charts):
  """Update |dest_charts| with |source_charts| and merge 'trace'.

  Args:
    dest_charts: A dictionary of charts, will hold the result.
    source_charts: A dictionary of charts.
  """

  for chart_name in source_charts:
    if chart_name == 'trace':
      if chart_name not in dest_charts:
        dest_charts[chart_name] = {}
      dest_charts[chart_name].update(source_charts[chart_name])
    else:
      dest_charts[chart_name] = source_charts[chart_name]

def _MergeControlChartJsonIntoEnabled(enabled_chart_json, control_chart_json):
  """Creates a diff chart_json from |enabled_chart_json| and
  |control_chart_json|, then append appropriated suffix to all three chart_json
  and merge them into |enabled_chart_json|.

  Args:
    enabled_chart_json: A dictionary of charts, will hold the result..
    control_chart_json: A dictionary of charts.
  """

  # Leaving fields as-is other than 'charts'
  enabled_charts = enabled_chart_json['charts']
  control_charts = control_chart_json['charts']
  diff_charts = copy.deepcopy(enabled_charts)
  diff_charts['trace'] = {}
  for chart_name in diff_charts.keys():
    if chart_name not in control_charts:
      # Charts like 'timeToInteractive_std' may not be there if all values are
      # None.
      del diff_charts[chart_name]
      continue
    for point_name in diff_charts[chart_name].keys():
      if point_name not in control_charts[chart_name]:
        del diff_charts[chart_name][point_name]
        continue
      _PointSubtraction(diff_charts[chart_name][point_name],
                        control_charts[chart_name][point_name])

  _RenameChartsAndPointsWithSuffix(enabled_charts, '_enabled')
  _RenameChartsAndPointsWithSuffix(control_charts, '_control')
  _RenameChartsAndPointsWithSuffix(diff_charts, '_diff')
  _MergeCharts(enabled_charts, control_charts)
  _MergeCharts(enabled_charts, diff_charts)

@benchmark.Info(emails=['juncai@chromium.org'])
class LoadingDesktopNetworkService(loading.LoadingDesktop):
  """Measures loading performance of desktop sites, with the network service
  enabled.

  Will run the test twice with feature on/off, and return the
  difference as well as the original results.
  """

  def __init__(self, max_failures=None):
    super(LoadingDesktopNetworkService, self).__init__(max_failures)
    self.enable_feature = False

  @classmethod
  def Name(cls):
    return 'loading.desktop.network_service'

  def Run(self, finder_options):
    """We shouldn't be overriding this according to
    telemetry.benchmark.Benchmark"""
    assert 'chartjson' in finder_options.output_formats, (
      'loading.desktop.network_service requires --output-format=chartjson. '
      'Please contact owner to rewrite the benchmark if chartjson is going '
      'away.')
    assert finder_options.output_dir
    output_dir = finder_options.output_dir
    temp_file_path = os.path.join(output_dir, 'results-chart.json')

    # Run test with feature disabled.
    self.enable_feature = False
    control_return_code = story_runner.RunBenchmark(self, finder_options)
    if control_return_code != 0:
      return control_return_code
    control_chart_json = json.load(open(temp_file_path))

    # Run test again with feature enabled.
    self.enable_feature = True
    enabled_return_code = story_runner.RunBenchmark(self, finder_options)
    if enabled_return_code != 0:
      return enabled_return_code
    enabled_chart_json = json.load(open(temp_file_path))

    logging.info('Starting to merge control chartjson into enabled chartjson')
    try:
      # Merge the result and compute the difference.
      _MergeControlChartJsonIntoEnabled(enabled_chart_json, control_chart_json)
    except Exception as e:
      logging.error('exception merging two chart json: %s', repr(e))
      traceback.print_exc()
      with open(temp_file_path, 'w') as f:
        json.dump({
          'control_chart_json': control_chart_json,
          'enabled_chart_json': enabled_chart_json},
          f, indent=2, separators=(',', ': '))
        f.write('\n')
        return 1
    else:
      logging.info('Finished merging chartjsons, writing back to disk')
      with open(temp_file_path, 'w') as f:
        json.dump(enabled_chart_json, f, indent=2, separators=(',', ': '))
        f.write('\n')
    return 0

  def SetExtraBrowserOptions(self, options):
    if not self.enable_feature:
      return

    enable_features_arg = '--enable-features=NetworkService'

    # If an "--enable-features" argument has been specified, append to the value
    # list of that argument.
    for arg in options.extra_browser_args:
      if arg.startswith('--enable-features='):
        options.extra_browser_args.remove(arg)
        enable_features_arg = arg + ',NetworkService'
        break

    options.AppendExtraBrowserArgs([enable_features_arg])
