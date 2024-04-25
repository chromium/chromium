# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Output formatter for JSON Test Results Format.

Format specification:
https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md
"""

import collections
import datetime
import json
import os

from core.results_processor import util


OUTPUT_FILENAME = 'test-results.json'


def ProcessIntermediateResults(test_results, options):
  """Process intermediate results and write output in output_dir."""
  results = Convert(test_results, options.output_dir, options.test_path_format)
  output_file = os.path.join(options.output_dir, OUTPUT_FILENAME)
  with open(output_file, 'w') as f:
    json.dump(results, f, sort_keys=True, indent=4, separators=(',', ': '))
  return output_file


def Convert(test_results, base_dir, test_path_format):
  """Convert intermediate results to the JSON Test Results Format.

  Args:
    test_results: The parsed intermediate results.
    base_dir: A string with the path to a base directory; artifact file paths
      will be written relative to this.

  Returns:
    A JSON serializable dict with the converted results.
  """
  results = {'tests': {}}
  status_counter = collections.Counter()

  for result in test_results:
    benchmark_name, story_name = util.SplitTestPath(result, test_path_format)
    actual_status = result['status']
    expected_status = actual_status if result['expected'] else 'PASS'
    status_counter[actual_status] += 1
    artifacts = result.get('outputArtifacts', {})
    shard = _GetTagValue(result.get('tags', []), 'shard', as_type=int)
    _MergeDict(
        results['tests'],
        {
            benchmark_name: {
                story_name: {
                    'actual': actual_status,
                    'expected': expected_status,
                    'is_unexpected': not result['expected'],
                    'times': float(result['runDuration'].rstrip('s')),
                    'shard': shard,
                    'artifacts': {
                        name: _ArtifactPath(artifact, base_dir)
                        for name, artifact in artifacts.items()
                    }
                }
            }
        }
    )

  for stories in results['tests'].values():
    for test in stories.values():
      test['actual'] = _DedupedStatus(test['actual'])
      test['expected'] = ' '.join(sorted(set(test['expected'])))
      test['is_unexpected'] = any(test['is_unexpected'])
      test['time'] = test['times'][0]
      test['shard'] = test['shard'][0]  # All shard values should be the same.
      if test['shard'] is None:
        del test['shard']

  # Test results are written in order of execution, so the first test start
  # time is approximately the start time of the whole suite.
  test_suite_start_time = (test_results[0]['startTime'] if test_results
                           else datetime.datetime.utcnow().isoformat() + 'Z')
  # If Telemetry stops with a unhandleable error, then remaining stories
  # are marked as unexpectedly skipped.
  interrupted = any(t['status'] == 'SKIP' and not t['expected']
                    for t in test_results)
  results.update(
      seconds_since_epoch=util.IsoTimestampToEpoch(test_suite_start_time),
      interrupted=interrupted,
      num_failures_by_type=dict(status_counter),
      path_delimiter='/',
      version=3,
  )

  return results


def _DedupedStatus(values):
  # TODO(crbug.com/40534832): The following logic is a workaround for how the
  # flakiness dashboard determines whether a test is flaky. As a test_case
  # (i.e. story) may be run multiple times, we squash as sequence of PASS
  # results to a single one. Note this does not affect the total number of
  # passes in num_failures_by_type.
  # See also crbug.com/1254733 for why we want to report a FAIL in the case
  # of flaky failures. A failed test run means less perf data so in general we
  # want to investigate and fix those failures, not dismiss them as flakes.
  deduped = set(values)
  if deduped == {'PASS'}:
    return 'PASS'
  if deduped == {'SKIP'}:
    return 'SKIP'
  if 'FAIL' in deduped:
    return 'FAIL'
  return ' '.join(values)


def _GetTagValue(tags, key, default=None, as_type=None):
  """Get the value of the first occurrence of a tag with a given key."""
  if as_type is None:
    as_type = lambda x: x
  return next((as_type(t['value']) for t in tags if t['key'] == key), default)


def _ArtifactPath(artifact, base_dir):
  """Extract either remote or local path of an artifact."""
  if 'fetchUrl' in artifact:
    return artifact['fetchUrl']
  # The spec calls for paths to be relative to the output directory and
  # '/'-delimited on all platforms.
  path = os.path.relpath(artifact['filePath'], base_dir)
  return path.replace(os.sep, '/')


def _MergeDict(target, values):
  # Is used to merge multiple runs of a story into a single test result.
  for key, value in values.items():
    if isinstance(value, dict):
      _MergeDict(target.setdefault(key, {}), value)
    elif isinstance(value, list):
      raise TypeError('Value to merge should not contain lists')
    else:  # i.e. a scalar value.
      target.setdefault(key, []).append(value)
