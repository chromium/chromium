# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file was copy-pasted over from:
# //build/scripts/slave/recipe_modules/swarming/resources/results_merger.py

# This file is responsbile for merging JSON test results in both the simplified
# JSON format and the Chromium JSON test results format version 3.

from __future__ import print_function

import copy
import json
import sys

import six

# These fields must appear in the test result output
REQUIRED = {
    'interrupted',
    'num_failures_by_type',
    'seconds_since_epoch',
    'tests',
    }

# These fields are optional, but must have the same value on all shards
OPTIONAL_MATCHING = (
    'builder_name',
    'build_number',
    'chromium_revision',
    'has_pretty_patch',
    'has_wdiff',
    'path_delimiter',
    'pixel_tests_enabled',
    'random_order_seed',
    )

OPTIONAL_IGNORED = (
    'layout_tests_dir',
    )

# These fields are optional and will be summed together
OPTIONAL_COUNTS = (
    'fixable',
    'num_flaky',
    'num_passes',
    'num_regressions',
    'skipped',
    'skips',
    )


class MergeException(Exception):
  pass


def merge_test_results(shard_results_list, test_cross_device=False):
  """ Merge list of results.

  Args:
    shard_results_list: list of results to merge. All the results must have the
      same format. Supported format are simplified JSON format & Chromium JSON
      test results format version 3 (see
      https://www.chromium.org/developers/the-json-test-results-format)
    test_cross_device: If true, some tests are running in multiple shards. This
      requires some extra handling on merging the values under 'tests'.

  Returns:
    a dictionary that represent the merged results. Its format follow the same
    format of all results in |shard_results_list|.
  """
  shard_results_list = [x for x in shard_results_list if x]
  if not shard_results_list:
    return {}

  if 'seconds_since_epoch' in shard_results_list[0]:
    return _merge_json_test_result_format(shard_results_list, test_cross_device)
  return _merge_simplified_json_format(shard_results_list)


def _merge_simplified_json_format(shard_results_list):
  # This code is specialized to the "simplified" JSON format that used to be
  # the standard for recipes.

  # These are the only keys we pay attention to in the output JSON.
  merged_results = {
    'successes': [],
    'failures': [],
    'valid': True,
  }

  for result_json in shard_results_list:
    successes = result_json.get('successes', [])
    failures = result_json.get('failures', [])
    valid = result_json.get('valid', True)

    if (not isinstance(successes, list) or not isinstance(failures, list) or
        not isinstance(valid, bool)):
      raise MergeException(
        'Unexpected value type in %s' % result_json)  # pragma: no cover

    merged_results['successes'].extend(successes)
    merged_results['failures'].extend(failures)
    merged_results['valid'] = merged_results['valid'] and valid
  return merged_results


def _merge_json_test_result_format(shard_results_list, test_cross_device=False):
  # This code is specialized to the Chromium JSON test results format version 3:
  # https://www.chromium.org/developers/the-json-test-results-format

  # These are required fields for the JSON test result format version 3.
  merged_results = {
    'tests': {},
    'interrupted': False,
    'version': 3,
    'seconds_since_epoch': float('inf'),
    'num_failures_by_type': {
    }
  }

  # To make sure that we don't mutate existing shard_results_list.
  shard_results_list = copy.deepcopy(shard_results_list)
  for result_json in shard_results_list:
    # TODO(tansell): check whether this deepcopy is actually necessary.
    result_json = copy.deepcopy(result_json)

    # Check the version first
    version = result_json.pop('version', -1)
    if version != 3:
      raise MergeException(  # pragma: no cover (covered by
                             # results_merger_unittest).
          'Unsupported version %s. Only version 3 is supported' % version)

    # Check the results for each shard have the required keys
    missing = REQUIRED - set(result_json)
    if missing:
      raise MergeException(  # pragma: no cover (covered by
                             # results_merger_unittest).
          'Invalid json test results (missing %s)' % missing)

    # Curry merge_values for this result_json.
    # pylint: disable=cell-var-from-loop
    merge = lambda key, merge_func: merge_value(
        result_json, merged_results, key, merge_func)

    if test_cross_device:
      # Results from the same test(story) may be found on different
      # shards(devices). We need to handle the merging on story level.
      merge('tests', merge_tries_v2)
    else:
      # Traverse the result_json's test trie & merged_results's test tries in
      # DFS order & add the n to merged['tests'].
      merge('tests', merge_tries)

    # If any were interrupted, we are interrupted.
    merge('interrupted', lambda x,y: x|y)

    # Use the earliest seconds_since_epoch value
    merge('seconds_since_epoch', min)

    # Sum the number of failure types
    merge('num_failures_by_type', sum_dicts)

    # Optional values must match
    for optional_key in OPTIONAL_MATCHING:
      if optional_key not in result_json:
        continue

      if optional_key not in merged_results:
        # Set this value to None, then blindly copy over it.
        merged_results[optional_key] = None
        merge(optional_key, lambda src, dst: src)
      else:
        merge(optional_key, ensure_match)

    # Optional values ignored
    for optional_key in OPTIONAL_IGNORED:
      if optional_key in result_json:
        merged_results[optional_key] = result_json.pop(
            # pragma: no cover (covered by
            # results_merger_unittest).
            optional_key)

    # Sum optional value counts
    for count_key in OPTIONAL_COUNTS:
      if count_key in result_json:  # pragma: no cover
        # TODO(mcgreevy): add coverage.
        merged_results.setdefault(count_key, 0)
        merge(count_key, lambda a, b: a+b)

    if result_json:
      raise MergeException(  # pragma: no cover (covered by
          # results_merger_unittest).
          'Unmergable values %s' % list(result_json.keys()))

  return merged_results


def merge_tries(source, dest):
  """ Merges test tries.

  This is intended for use as a merge_func parameter to merge_value.

  Args:
      source: A result json test trie.
      dest: A json test trie merge destination.
  """
  # merge_tries merges source into dest by performing a lock-step depth-first
  # traversal of dest and source.
  # pending_nodes contains a list of all sub-tries which have been reached but
  # need further merging.
  # Each element consists of a trie prefix, and a sub-trie from each of dest
  # and source which is reached via that prefix.
  pending_nodes = [('', dest, source)]
  while pending_nodes:
    prefix, dest_node, curr_node = pending_nodes.pop()
    for k, v in curr_node.items():
      if k in dest_node:
        if not isinstance(v, dict):
          raise MergeException(
              '%s:%s: %r not mergable, curr_node: %r\ndest_node: %r' %
              (prefix, k, v, curr_node, dest_node))
        pending_nodes.append(("%s:%s" % (prefix, k), dest_node[k], v))
      else:
        dest_node[k] = v
  return dest


def merge_tries_v2(source, dest):
  """ Merges test tries, and adds support for merging results for the same story
  from different devices, which is not supported on v1.

  This is intended for use as a merge_func parameter to merge_value.

  Args:
      source: A result json test trie.
      dest: A json test trie merge destination.
  """
  # merge_tries merges source into dest by performing a lock-step depth-first
  # traversal of dest and source.
  # pending_nodes contains a list of all sub-tries which have been reached but
  # need further merging.
  # Each element consists of a trie prefix, and a sub-trie from each of dest
  # and source which is reached via that prefix.
  pending_nodes = [('', dest, source)]
  while pending_nodes:
    prefix, dest_node, curr_node = pending_nodes.pop()
    for k, v in curr_node.items():
      if k in dest_node:
        if not isinstance(v, dict):
          raise MergeException(
              '%s:%s: %r not mergable, curr_node: %r\ndest_node: %r' %
              (prefix, k, v, curr_node, dest_node))
        if 'actual' in v and 'expected' in v:
          # v is test result of a story name which is already in dest
          _merging_cross_device_results(v, dest_node[k])
        else:
          pending_nodes.append(("%s:%s" % (prefix, k), dest_node[k], v))
      else:
        dest_node[k] = v
  return dest


def _merging_cross_device_results(src, dest):
  # 1. Merge the 'actual' field and update the is_unexpected based on new values
  dest['actual'] += ' %s' % src['actual']
  if any(actual != dest['expected'] for actual in dest['actual'].split()):
    dest['is_unexpected'] = True
  # 2. append each item under the 'artifacts' and 'times'.
  if 'artifacts' in src:
    if 'artifacts' in dest:
      for artifact, artifact_list in src['artifacts'].items():
        if artifact in dest['artifacts']:
          dest['artifacts'][artifact] += artifact_list
        else:
          dest['artifacts'][artifact] = artifact_list
    else:
      dest['artifacts'] = src['artifacts']
  if 'times' in src:
    if 'times' in dest:
      dest['times'] += src['times']
    else:
      dest['time'] = src['time']
      dest['times'] = src['times']
  # 3. remove the 'shard' because now the results are from multiple shards.
  if 'shard' in dest:
    del dest['shard']


def ensure_match(source, dest):
  """ Returns source if it matches dest.

  This is intended for use as a merge_func parameter to merge_value.

  Raises:
      MergeException if source != dest
  """
  if source != dest:
    raise MergeException(  # pragma: no cover (covered by
                           # results_merger_unittest).
        "Values don't match: %s, %s" % (source, dest))
  return source


def sum_dicts(source, dest):
  """ Adds values from source to corresponding values in dest.

  This is intended for use as a merge_func parameter to merge_value.
  """
  for k, v in source.items():
    dest.setdefault(k, 0)
    dest[k] += v

  return dest


def merge_value(source, dest, key, merge_func):
  """ Merges a value from source to dest.

  The value is deleted from source.

  Args:
    source: A dictionary from which to pull a value, identified by key.
    dest: The dictionary into to which the value is to be merged.
    key: The key which identifies the value to be merged.
    merge_func(src, dst): A function which merges its src into dst,
        and returns the result. May modify dst. May raise a MergeException.

  Raises:
    MergeException if the values can not be merged.
  """
  try:
    dest[key] = merge_func(source[key], dest[key])
  except MergeException as e:
    # The message attribute does not exist in Python 3, but Python 3's exception
    # chaining should get us equivalent functionality.
    if six.PY2:
      e.message = "MergeFailure for %s\n%s" % (key, e.message)  # pylint: disable=attribute-defined-outside-init
      e.args = tuple([e.message] + list(e.args[1:]))
      raise
    raise MergeException('MergeFailure for %s\n%s' % (key, e))
  del source[key]


def main(files):
  if len(files) < 2:
    sys.stderr.write("Not enough JSON files to merge.\n")
    return 1
  sys.stderr.write('Starting with %s\n' % files[0])
  result = json.load(open(files[0]))
  for f in files[1:]:
    sys.stderr.write('Merging %s\n' % f)
    result = merge_test_results([result, json.load(open(f))])
  print(json.dumps(result))
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
