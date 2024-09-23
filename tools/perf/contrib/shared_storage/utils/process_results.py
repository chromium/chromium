# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import Counter
import json
import logging
import os
import subprocess
import traceback

from .file_util import (GetBenchmarkDBSize, GetDataDir,
                        GetExpectedHistogramsDictionary,
                        GetExpectedHistogramsFile, GetHistogramCountDeltasFile,
                        GetProcessedFiles, GetProcessor, GetRawHistogramsFile,
                        GetRunPathFile, MovePreviousHistogramsFiles)
from .histogram_list import GetSharedStorageUmaHistograms
from .util import GetNonePlaceholder, JsonDump


def _ProcessHistograms(histogram_data):
  if not isinstance(histogram_data, list):
    raise RuntimeError(
        'Expected histogram_data to be type list but got type %s' %
        type(histogram_data))

  guid_map = {}
  histogram_info = {}
  expected_processed_all_guids = False
  benchmark_name = None

  for item in histogram_data:
    if not isinstance(item, dict):
      raise RuntimeError('Expected item to be type dict but got type %s' %
                         type(item))

    if 'guid' in item:
      if expected_processed_all_guids:
        info = ('; %s\nitem[`guid`]: %s' %
                (JsonDump(item), item.get('guid', '[[Not Found]]')))
        msg = 'Expected to process all items with `guid` before any with `name`'
        raise RuntimeError(msg + info)
      if len(item) != 3:
        raise RuntimeError(
            'Expected item with `guid` to have length 3; got item: %s' %
            JsonDump(item))
      if 'type' not in item:
        raise RuntimeError(
            'Expected item with `guid` to have key `type`; got: %s' %
            JsonDump(item))
      [value_key] = [key for key in item if key not in ('guid', 'type')]
      guid_map[item['guid']] = item[value_key]

    elif 'name' in item:
      expected_processed_all_guids = True
      name = item['name']
      if not name in GetSharedStorageUmaHistograms():
        continue
      for key in ['diagnostics', 'running', 'sampleValues']:
        if key not in item:
          raise RuntimeError(
              'Expected item with `name` to have key `%s`; got: %s' %
              (key, JsonDump(item)))

      histogram_info.setdefault(name, [])
      item_diagnostics = item['diagnostics']
      diagnostics = {
          key: guid_map.get(item_diagnostics[key], item_diagnostics[key])
          for key in item_diagnostics
      }
      item_info = {
          'diagnostics': diagnostics,
          'running': item['running'],
          'sampleValues': item['sampleValues']
      }
      histogram_info[name].append(item_info)
      if not benchmark_name:
        if (not 'benchmarks' in diagnostics
            or len(diagnostics['benchmarks']) != 1):
          RuntimeError(
              'Expected `diagnostics` to have `benchmarks` of length 1; got %s'
              % JsonDump(diagnostics))
        benchmark_name = diagnostics['benchmarks'][0]

  agg_histograms = {}
  by_story = {}
  counts = {}

  for histogram, info_list in histogram_info.items():
    agg_histograms.setdefault(histogram, [])

    for entry in info_list:
      for key in ['diagnostics', 'sampleValues']:
        if key not in entry:
          raise RuntimeError('Entry %s does not have key `%s`' %
                             (JsonDump(entry), key))
      for key in ['stories', 'storysetRepeats']:
        if key not in entry['diagnostics']:
          raise RuntimeError('Entry diagnostics %s does not have key `%s`' %
                             (JsonDump(entry), key))
        if len(entry['diagnostics'][key]) != 1:
          raise RuntimeError(
              'Entry diagnostics %s expected `%s` to lave length 1' %
              (JsonDump(entry), key))

      agg_histograms[histogram] += entry['sampleValues']

      [story] = entry['diagnostics']['stories']
      [rep] = entry['diagnostics']['storysetRepeats']

      by_story.setdefault(story, {}).setdefault(rep,
                                                {}).setdefault(histogram, [])
      by_story[story][rep][histogram] += entry['sampleValues']
      counts.setdefault(story, {}).setdefault(rep, Counter())
      counts[story][rep][histogram] += len(entry['sampleValues'])

  runs_all_match, deltas = _CheckHistogramCounts(counts)
  results = [histogram_info, agg_histograms, by_story, counts, deltas]
  size = GetBenchmarkDBSize(benchmark_name)
  return runs_all_match, size, list(map(JsonDump, results))


def _CheckHistogramCounts(actual_counts):
  counts_match = True
  expected_counts = GetExpectedHistogramsDictionary()
  missed_stories = (set(expected_counts.keys()).difference(
      set(actual_counts.keys())))
  if len(missed_stories) > 0:
    counts_match = False
    logging.warning('Missed recording for stories %s' %
                    repr(list(missed_stories)))
  unexpected_stories = (set(actual_counts.keys()).difference(
      set(expected_counts.keys())))
  if len(missed_stories) > 0:
    counts_match = False
    logging.warning('Unexpectedly recorded for stories %s' %
                    repr(list(unexpected_stories)))

  deltas = {}
  for story, repeats in actual_counts.items():
    for repeat, histogram_counts in repeats.items():
      if histogram_counts == expected_counts[story]:
        continue
      counts_match = False
      deltas.setdefault(story, {}).setdefault(repeat, Counter())
      missed_histograms = (set(expected_counts[story].keys()).difference(
          set(histogram_counts.keys())))
      for histogram in missed_histograms:
        expected_count = expected_counts[story][histogram]
        deltas[story][repeat][histogram] = -expected_count
      for histogram, count in histogram_counts.items():
        expected_count = expected_counts[story].get(histogram, 0)
        if count != expected_count:
          deltas[story][repeat][histogram] = count - expected_count
      logging.warning(
          'Story %s, repeat %s failed to record expected histogram counts' %
          (story, repeat))
      logging.warning('Expected counts: %s' % JsonDump(expected_counts[story]))
      logging.warning('Actual counts: %s' % JsonDump(histogram_counts))

  return counts_match, deltas


def _ProcessResultsInternal():
  if not os.path.exists(GetDataDir()) or not os.path.isdir(GetDataDir()):
    raise RuntimeError(
        'Data directory %s does not exist or is not a directory' %
        (GetDataDir()))

  if (not os.path.exists(GetRunPathFile())
      or not os.path.isfile(GetRunPathFile())):
    raise RuntimeError('Run path file %s does not exist or is not a file' %
                       GetRunPathFile())

  run_results_path = None
  with open(GetRunPathFile(), 'r') as f:
    run_results_path = f.read()
  none_placeholder = GetNonePlaceholder()
  if not run_results_path or run_results_path.find(none_placeholder) > -1:
    raise RuntimeError('Directory path of perf results not found')

  MovePreviousHistogramsFiles()

  subprocess.check_call([
      GetProcessor(), "--output-format=histograms",
      "--intermediate-dir=" + run_results_path
  ])

  if (not os.path.exists(GetRawHistogramsFile())
      or not os.path.isfile(GetRawHistogramsFile())):
    raise RuntimeError('Histogram results %s not found or is not a file' %
                       GetRawHistogramsFile())

  histogram_data = None
  with open(GetRawHistogramsFile(), 'r') as f:
    histogram_data = json.load(f)

  runs_all_match, size, results = _ProcessHistograms(histogram_data)
  print("Benchmark's initial database size: %s" % size)
  if runs_all_match:
    print('SUCCESS: Actual histogram counts are equal to expected counts ' +
          'for all stories and repeats.')
  else:
    print('WARNING: Actual histogram counts differ from expected counts for ' +
          'at least one story/repeat. See file://%s for the deltas.' %
          GetHistogramCountDeltasFile())
  print('View expected histogram counts at file://%s' %
        GetExpectedHistogramsFile())
  for (result, filename) in zip(results, GetProcessedFiles()):
    if filename == GetHistogramCountDeltasFile():
      json_result = json.loads(result)
      if runs_all_match and len(json_result) == 0:
        continue
      if runs_all_match:
        logging.warning(('Unexpectedly got histogram count deltas %s when ' %
                         result) +
                        'all runs were reported to have counts match')
      elif len(json_result) == 0:
        logging.warning(
            'A discrepancy in run counts was reported, but the returned ' +
            'deltas object was empty')
    with open(filename, 'w') as f:
      f.write(result)
      print('View processed results at file://%s' % filename)


def ProcessResults():
  try:
    _ProcessResultsInternal()
    return 0
  except Exception as e:  # pylint: disable=broad-except
    logging.warning('Encountered exception: %s\n%s' %
                    (repr(e), traceback.format_exc()))
    return 1
