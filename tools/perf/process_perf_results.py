#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import multiprocessing as mp
import os
from os import listdir
from os.path import isfile, join, basename
import shutil
import sys
import tempfile
import time
import uuid

from core import path_util
from core import upload_results_to_perf_dashboard
from core import results_merger

path_util.AddAndroidPylibToPath()

try:
  from pylib.utils import logdog_helper
except ImportError:
  pass


RESULTS_URL = 'https://chromeperf.appspot.com'

# Until we are migrated to LUCI, we will be utilizing a hard
# coded master name based on what is passed in in the build properties.
# See crbug.com/801289 for more details.
MACHINE_GROUP_JSON_FILE = os.path.join(
      path_util.GetChromiumSrcDir(), 'tools', 'perf', 'core',
      'perf_dashboard_machine_group_mapping.json')

JSON_CONTENT_TYPE = 'application/json'


def _GetMachineGroup(build_properties):
  machine_group = None
  if build_properties.get('perf_dashboard_machine_group', False):
    # Once luci migration is complete this will exist as a property
    # in the build properties
    machine_group =  build_properties['perf_dashboard_machine_group']
  else:
    mastername_mapping = {}
    with open(MACHINE_GROUP_JSON_FILE) as fp:
      mastername_mapping = json.load(fp)
      legacy_mastername = build_properties['mastername']
      if mastername_mapping.get(legacy_mastername):
        machine_group = mastername_mapping[legacy_mastername]
  if not machine_group:
    raise ValueError(
        'Must set perf_dashboard_machine_group or have a valid '
        'mapping in '
        'src/tools/perf/core/perf_dashboard_machine_group_mapping.json'
        'See bit.ly/perf-dashboard-machine-group for more details')
  return machine_group


def _upload_perf_results(json_to_upload, name, configuration_name,
    build_properties, service_account_file, output_json_file):
  """Upload the contents of result JSON(s) to the perf dashboard."""
  args= [
      '--buildername', build_properties['buildername'],
      '--buildnumber', build_properties['buildnumber'],
      '--name', name,
      '--configuration-name', configuration_name,
      '--results-file', json_to_upload,
      '--results-url', RESULTS_URL,
      '--got-revision-cp', build_properties['got_revision_cp'],
      '--got-v8-revision', build_properties['got_v8_revision'],
      '--got-webrtc-revision', build_properties['got_webrtc_revision'],
      '--output-json-file', output_json_file,
      '--perf-dashboard-machine-group', _GetMachineGroup(build_properties)
  ]
  is_luci = False
  buildbucket = build_properties.get('buildbucket', {})
  if isinstance(buildbucket, basestring):
    buildbucket = json.loads(buildbucket)
  if ('build' in buildbucket and
      buildbucket['build'].get('bucket') == 'luci.chrome.ci'):
    is_luci = True

  if 'build' in buildbucket:
    args += [
      '--project', buildbucket['build'].get('project'),
      '--buildbucket', buildbucket['build'].get('bucket'),
    ]

  if service_account_file and not is_luci:
    args += ['--service-account-file', service_account_file]

  if build_properties.get('git_revision'):
    args.append('--git-revision')
    args.append(build_properties['git_revision'])
  if _is_histogram(json_to_upload):
    args.append('--send-as-histograms')

  return upload_results_to_perf_dashboard.main(args)

def _is_histogram(json_file):
  with open(json_file) as f:
    data = json.load(f)
    return isinstance(data, list)
  return False


def _merge_json_output(output_json, jsons_to_merge, extra_links):
  """Merges the contents of one or more results JSONs.

  Args:
    output_json: A path to a JSON file to which the merged results should be
      written.
    jsons_to_merge: A list of JSON files that should be merged.
    extra_links: a (key, value) map in which keys are the human-readable strings
      which describe the data, and value is logdog url that contain the data.
  """
  begin_time = time.time()
  merged_results = results_merger.merge_test_results(jsons_to_merge)

  # Only append the perf results links if present
  if extra_links:
    merged_results['links'] = extra_links

  with open(output_json, 'w') as f:
    json.dump(merged_results, f)

  end_time = time.time()
  print_duration('Merging json test results', begin_time, end_time)
  return 0


def _handle_perf_json_test_results(
    benchmark_directory_map, test_results_list):
  begin_time = time.time()
  benchmark_enabled_map = {}
  for benchmark_name, directories in benchmark_directory_map.iteritems():
    for directory in directories:
      # Obtain the test name we are running
      is_ref = '.reference' in benchmark_name
      enabled = True
      with open(join(directory, 'test_results.json')) as json_data:
        json_results = json.load(json_data)
        if not json_results:
          # Output is null meaning the test didn't produce any results.
          # Want to output an error and continue loading the rest of the
          # test results.
          print 'No results produced for %s, skipping upload' % directory
          continue
        if json_results.get('version') == 3:
          # Non-telemetry tests don't have written json results but
          # if they are executing then they are enabled and will generate
          # chartjson results.
          if not bool(json_results.get('tests')):
            enabled = False
        if not is_ref:
          # We don't need to upload reference build data to the
          # flakiness dashboard since we don't monitor the ref build
          test_results_list.append(json_results)
      if not enabled:
        # We don't upload disabled benchmarks or tests that are run
        # as a smoke test
        print 'Benchmark %s disabled' % benchmark_name
      benchmark_enabled_map[benchmark_name] = enabled

  end_time = time.time()
  print_duration('Analyzing perf json test results', begin_time, end_time)
  return benchmark_enabled_map


def _generate_unique_logdog_filename(name_prefix):
  return name_prefix + '_' + str(uuid.uuid4())


def _handle_perf_logs(benchmark_directory_map, extra_links):
  """ Upload benchmark logs to logdog and add a page entry for them. """
  begin_time = time.time()
  benchmark_logs_links = {}

  for benchmark_name, directories in benchmark_directory_map.iteritems():
    for directory in directories:
      with open(join(directory, 'benchmark_log.txt')) as f:
        uploaded_link = logdog_helper.text(
            name=_generate_unique_logdog_filename(benchmark_name),
            data=f.read())
        if benchmark_name in benchmark_logs_links.keys():
          benchmark_logs_links[benchmark_name].append(uploaded_link)
        else:
          benchmark_logs_links[benchmark_name] = [uploaded_link]

  logdog_file_name = _generate_unique_logdog_filename('Benchmarks_Logs')
  logdog_stream = logdog_helper.text(
      logdog_file_name, json.dumps(benchmark_logs_links, sort_keys=True,
                                   indent=4, separators=(',', ': ')),
      content_type=JSON_CONTENT_TYPE)
  extra_links['Benchmarks logs'] = logdog_stream
  end_time = time.time()
  print_duration('Generating perf log streams', begin_time, end_time)


def _handle_benchmarks_shard_map(benchmarks_shard_map_file, extra_links):
  begin_time = time.time()
  with open(benchmarks_shard_map_file) as f:
    benchmarks_shard_data = json.load(f)
    logdog_file_name = _generate_unique_logdog_filename('Benchmarks_Shard_Map')
    logdog_stream = logdog_helper.text(
        logdog_file_name, json.dumps(benchmarks_shard_data, sort_keys=True,
                                     indent=4, separators=(',', ': ')),
        content_type=JSON_CONTENT_TYPE)
    extra_links['Benchmarks shard map'] = logdog_stream
  end_time = time.time()
  print_duration('Generating benchmark shard map stream', begin_time, end_time)


def _get_benchmark_name(directory):
  return basename(directory).replace(" benchmark", "")


def process_perf_results(output_json, configuration_name,
                         service_account_file,
                         build_properties, task_output_dir,
                         smoke_test_mode):
  """Process perf results.

  Consists of merging the json-test-format output, uploading the perf test
  output (chartjson and histogram), and store the benchmark logs in logdog.

  Each directory in the task_output_dir represents one benchmark
  that was run. Within this directory, there is a subdirectory with the name
  of the benchmark that was run. In that subdirectory, there is a
  perftest-output.json file containing the performance results in histogram
  or dashboard json format and an output.json file containing the json test
  results for the benchmark.

  Returns:
    (return_code, upload_results_map):
      return_code is 0 if the whole operation is successful, non zero otherwise.
      benchmark_upload_result_map: the dictionary that describe which benchmarks
        were successfully uploaded.
  """
  begin_time = time.time()
  return_code = 0
  benchmark_upload_result_map = {}
  directory_list = [
      f for f in listdir(task_output_dir)
      if not isfile(join(task_output_dir, f))
  ]
  benchmark_directory_list = []
  benchmarks_shard_map_file = None
  for directory in directory_list:
    for f in listdir(join(task_output_dir, directory)):
      path = join(task_output_dir, directory, f)
      if path.endswith('benchmarks_shard_map.json'):
        benchmarks_shard_map_file = path
      else:
        benchmark_directory_list.append(path)

  # Now create a map of benchmark name to the list of directories
  # the lists were written to.
  benchmark_directory_map = {}
  for directory in benchmark_directory_list:
    benchmark_name = _get_benchmark_name(directory)
    if benchmark_name in benchmark_directory_map.keys():
      benchmark_directory_map[benchmark_name].append(directory)
    else:
      benchmark_directory_map[benchmark_name] = [directory]

  test_results_list = []

  build_properties = json.loads(build_properties)
  if not configuration_name:
    # we are deprecating perf-id crbug.com/817823
    configuration_name = build_properties['buildername']

  extra_links = {}

  # First, upload benchmarks shard map to logdog and add a page
  # entry for it in extra_links.
  if benchmarks_shard_map_file:
    _handle_benchmarks_shard_map(benchmarks_shard_map_file, extra_links)

  # Second, upload all the benchmark logs to logdog and add a page entry for
  # those links in extra_links.
  _handle_perf_logs(benchmark_directory_map, extra_links)

  # Then try to obtain the list of json test results to merge
  # and determine the status of each benchmark.
  benchmark_enabled_map = _handle_perf_json_test_results(
      benchmark_directory_map, test_results_list)

  if not smoke_test_mode:
    try:
      return_code, benchmark_upload_result_map = _handle_perf_results(
          benchmark_enabled_map, benchmark_directory_map,
          configuration_name, build_properties, service_account_file,
          extra_links)
    except Exception:
      logging.exception('Error handling perf results jsons')
      return_code = 1

  # Finally, merge all test results json, add the extra links and write out to
  # output location
  _merge_json_output(output_json, test_results_list, extra_links)
  end_time = time.time()
  print_duration('Total process_perf_results', begin_time, end_time)
  return return_code, benchmark_upload_result_map

def _merge_chartjson_results(chartjson_dicts):
  merged_results = chartjson_dicts[0]
  for chartjson_dict in chartjson_dicts[1:]:
    for key in chartjson_dict:
      if key == 'charts':
        for add_key in chartjson_dict[key]:
          merged_results[key][add_key] = chartjson_dict[key][add_key]
  return merged_results

def _merge_histogram_results(histogram_lists):
  merged_results = []
  for histogram_list in histogram_lists:
    merged_results += histogram_list

  return merged_results

def _merge_perf_results(benchmark_name, results_filename, directories):
  begin_time = time.time()
  collected_results = []
  for directory in directories:
    filename = join(directory, 'perf_results.json')
    with open(filename) as pf:
      collected_results.append(json.load(pf))

  # Assuming that multiple shards will only be chartjson or histogram set
  # Non-telemetry benchmarks only ever run on one shard
  merged_results = []
  if isinstance(collected_results[0], dict):
    merged_results = _merge_chartjson_results(collected_results)
  elif isinstance(collected_results[0], list):
    merged_results =_merge_histogram_results(collected_results)

  with open(results_filename, 'w') as rf:
    json.dump(merged_results, rf)

  end_time = time.time()
  print_duration(('%s results merging' % (benchmark_name)),
                 begin_time, end_time)


def _upload_individual(
    benchmark_name, directories, configuration_name,
    build_properties, output_json_file, service_account_file):
  tmpfile_dir = tempfile.mkdtemp()
  try:
    upload_begin_time = time.time()
    # There are potentially multiple directores with results, re-write and
    # merge them if necessary
    results_filename = None
    if len(directories) > 1:
      merge_perf_dir = os.path.join(
          os.path.abspath(tmpfile_dir), benchmark_name)
      if not os.path.exists(merge_perf_dir):
        os.makedirs(merge_perf_dir)
      results_filename = os.path.join(
          merge_perf_dir, 'merged_perf_results.json')
      _merge_perf_results(benchmark_name, results_filename, directories)
    else:
      # It was only written to one shard, use that shards data
      results_filename = join(directories[0], 'perf_results.json')

    results_size_in_mib = os.path.getsize(results_filename) / (2 ** 20)
    print 'Uploading perf results from %s benchmark (size %s Mib)' % (
        benchmark_name, results_size_in_mib)
    with open(output_json_file, 'w') as oj:
      upload_return_code = _upload_perf_results(
        results_filename,
        benchmark_name, configuration_name, build_properties,
        service_account_file, oj)
      upload_end_time = time.time()
      print_duration(('%s upload time' % (benchmark_name)),
                     upload_begin_time, upload_end_time)
      return (benchmark_name, upload_return_code == 0)
  finally:
    shutil.rmtree(tmpfile_dir)


def _upload_individual_benchmark(params):
  try:
    return _upload_individual(*params)
  except Exception:
    benchmark_name = params[0]
    upload_succeed = False
    logging.exception('Error uploading perf result of %s' % benchmark_name)
    return benchmark_name, upload_succeed


def _handle_perf_results(
    benchmark_enabled_map, benchmark_directory_map, configuration_name,
    build_properties, service_account_file, extra_links):
  """
    Upload perf results to the perf dashboard.

    This method also upload the perf results to logdog and augment it to
    |extra_links|.

    Returns:
      (return_code, benchmark_upload_result_map)
      return_code is 0 if this upload to perf dashboard successfully, 1
        otherwise.
       benchmark_upload_result_map is a dictionary describes which benchmark
        was successfully uploaded.
  """
  begin_time = time.time()
  tmpfile_dir = tempfile.mkdtemp('outputresults')
  try:
    # Upload all eligible benchmarks to the perf dashboard
    results_dict = {}

    invocations = []
    for benchmark_name, directories in benchmark_directory_map.iteritems():
      if not benchmark_enabled_map.get(benchmark_name, False):
        continue
      # Create a place to write the perf results that you will write out to
      # logdog.
      output_json_file = os.path.join(
          tmpfile_dir, (str(uuid.uuid4()) + benchmark_name))
      results_dict[benchmark_name] = output_json_file
      invocations.append((
          benchmark_name, directories, configuration_name,
          build_properties, output_json_file, service_account_file))

    # Kick off the uploads in mutliple processes
    pool = mp.Pool()
    try:
      async_result = pool.map_async(
          _upload_individual_benchmark, invocations)
      results = async_result.get(timeout=2000)
    except mp.TimeoutError:
      logging.error('Failed uploading benchmarks to perf dashboard in parallel')
      pool.terminate()
      results = []
      for benchmark_name in benchmark_directory_map:
        results.append((benchmark_name, False))

    # Keep a mapping of benchmarks to their upload results
    benchmark_upload_result_map = {}
    for r in results:
      benchmark_upload_result_map[r[0]] = r[1]

    logdog_dict = {}
    upload_failures_counter = 0
    logdog_stream = None
    logdog_label = 'Results Dashboard'
    for benchmark_name, output_file in results_dict.iteritems():
      upload_succeed = benchmark_upload_result_map[benchmark_name]
      if not upload_succeed:
        upload_failures_counter += 1
      is_reference = '.reference' in benchmark_name
      _write_perf_data_to_logfile(
        benchmark_name, output_file,
        configuration_name, build_properties, logdog_dict,
        is_reference, upload_failure=not upload_succeed)

    logdog_file_name = _generate_unique_logdog_filename('Results_Dashboard_')
    logdog_stream = logdog_helper.text(logdog_file_name,
        json.dumps(dict(logdog_dict), sort_keys=True,
                   indent=4, separators=(',', ': ')),
        content_type=JSON_CONTENT_TYPE)
    if upload_failures_counter > 0:
      logdog_label += ('Upload Failure (%s benchmark upload failures)' %
                       upload_failures_counter)
    extra_links[logdog_label] = logdog_stream
    end_time = time.time()
    print_duration('Uploading results to perf dashboard', begin_time, end_time)
    if upload_failures_counter > 0:
      return 1, benchmark_upload_result_map
    return 0, benchmark_upload_result_map
  finally:
    shutil.rmtree(tmpfile_dir)


def _write_perf_data_to_logfile(benchmark_name, output_file,
    configuration_name, build_properties,
    logdog_dict, is_ref, upload_failure):
  viewer_url = None
  # logdog file to write perf results to
  if os.path.exists(output_file):
    output_json_file = logdog_helper.open_text(benchmark_name)
    with open(output_file) as f:
      try:
        results = json.load(f)
        json.dump(results, output_json_file,
                indent=4, separators=(',', ': '))
      except ValueError:
        print ('Error parsing perf results JSON for benchmark  %s' %
               benchmark_name)

    output_json_file.close()
    viewer_url = output_json_file.get_viewer_url()
  else:
    print ("Perf results JSON file doesn't exist for benchmark %s" %
           benchmark_name)

  base_benchmark_name = benchmark_name.replace('.reference', '')

  if base_benchmark_name not in logdog_dict:
    logdog_dict[base_benchmark_name] = {}

  # add links for the perf results and the dashboard url to
  # the logs section of buildbot
  if is_ref:
    logdog_dict[base_benchmark_name]['perf_results_ref'] = viewer_url
    if upload_failure:
      logdog_dict[base_benchmark_name]['ref_upload_failed'] = 'True'
  else:
    logdog_dict[base_benchmark_name]['dashboard_url'] = (
        upload_results_to_perf_dashboard.GetDashboardUrl(
            benchmark_name,
            configuration_name, RESULTS_URL,
            build_properties['got_revision_cp'],
            _GetMachineGroup(build_properties)))
    logdog_dict[base_benchmark_name]['perf_results'] = viewer_url
    if upload_failure:
      logdog_dict[base_benchmark_name]['upload_failed'] = 'True'


def print_duration(step, start, end):
  print 'Duration of %s: %d seconds' % (step, end-start)

def main():
  """ See collect_task.collect_task for more on the merge script API. """
  parser = argparse.ArgumentParser()
  # configuration-name (previously perf-id) is the name of bot the tests run on
  # For example, buildbot-test is the name of the android-go-perf bot
  # configuration-name and results-url are set in the json file which is going
  # away tools/perf/core/chromium.perf.fyi.extras.json
  parser.add_argument('--configuration-name', help=argparse.SUPPRESS)
  parser.add_argument('--service-account-file', help=argparse.SUPPRESS,
                      default=None)

  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('-o', '--output-json', required=True,
                      help=argparse.SUPPRESS)
  parser.add_argument('json_files', nargs='*', help=argparse.SUPPRESS)
  parser.add_argument('--smoke-test-mode', action='store_true',
                      help='This test should be run in smoke test mode'
                      ' meaning it does not upload to the perf dashboard')

  args = parser.parse_args()

  return_code, _ = process_perf_results(
      args.output_json, args.configuration_name,
      args.service_account_file,
      args.build_properties, args.task_output_dir,
      args.smoke_test_mode)
  return return_code


if __name__ == '__main__':
  sys.exit(main())
