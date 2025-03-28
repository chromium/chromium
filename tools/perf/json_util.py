# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils to convert json from result2 to skia."""

import collections
import dataclasses
import datetime
import json
import logging
import os
import re
import statistics
from typing import Any, Dict, List, Mapping, Optional, Set, Tuple
from core import path_util

import json_constants

# The source of truth for public perf builders, which is extracted from
# datastore (url http://shortn/_ApO0FuH9pg and url http://shortn/_pKoMUP1fe6).
PUBLIC_PERF_BUILDERS_PATH = os.path.join(
      path_util.GetChromiumSrcDir(), "tools", "perf",
      "public_builders.json")


def is_public_builder(builder_name: str) -> bool:
  """Returns whether the builder is public.

  Args:
    builder_name: The builder name.
  Returns:
    Whether the builder is public.
  """
  if not builder_name:
    # Return True if the builder name is empty so it only uploads to a public
    # bucket.
    return True
  with open(PUBLIC_PERF_BUILDERS_PATH, "r") as fp:
    public_builders = json.load(fp)
  return builder_name in public_builders["public_perf_builders"]


def gcs_buckets_from_builder_name(
    builder_name: str,
    master_name: str,
    experiment_only: bool=False,
    public_copy_to_experiment: bool=False) -> List[str]:
  """Returns the GCS buckets to upload the json to.

  Args:
    builder_name: The builder name.
    master_name: The master name.
    experiment_only: Whether the json is to uoload for experiment only.
    public_copy_to_experiment: Whether to copy the public data to experiment.
  Returns:
    The GCS buckets to upload the json to.
  """
  if experiment_only:
    # Hardcoded for a/b testing to achieve data parity.
    return [json_constants.EXPERIMENT_GCS_BUCKET]
  if not builder_name or not master_name:
    return []
  is_public = is_public_builder(builder_name)
  for _, value in json_constants.REPOSITORY_PROPERTY_MAP.items():
    if master_name in value["masters"]:
      if public_copy_to_experiment and is_public:
        return [value["public_bucket_name"],
                value["internal_bucket_name"],
                json_constants.EXPERIMENT_GCS_BUCKET]
      if is_public:
        return [value["public_bucket_name"], value["internal_bucket_name"]]
      return [value["internal_bucket_name"]]
  return []


def calculate_stats(values):
  """Calculates the mean, std dev, count, max, min, and sum of a list of values.

  Args:
    values: A list of values.
  Returns:
    A tuple of the mean, std dev, count, max, min, and sum of the values.
  """
  filtered_values = [value for value in values if value is not None]
  n = len(filtered_values)
  if n == 0:
    return 0, 0, 0, 0, 0, 0

  average = sum(filtered_values) / n
  # If there is only one value, the standard deviation is 0.
  std_dev = (statistics.stdev(filtered_values)
             if len(filtered_values) > 1 else 0.0)
  return (average, std_dev, n, max(filtered_values), min(filtered_values),
          sum(filtered_values))


def extract_subtest_from_stories_tags(stories: List[str],
                                      tags: List[str]) -> Tuple[str, str]:
  """Extracts two specific values from a given stories and storyTags."""
  tags_to_use = [t.split(":") for t in tags if ":" in t]
  if not tags_to_use and stories:
    return stories[0], ""
  subtest_1 = "_".join(v for _, v in sorted(tags_to_use))
  array = [t.split(":") for t in stories if ":" in t]
  stories_parts = [item for sub in array for item in sub]
  subtest_2 = "_".join(stories_parts)
  return subtest_1, subtest_2


@dataclasses.dataclass
class PerfBuilderDetails:
  """Details of build_properties on a perf builder/tester."""

  bot: str
  builder_page: str
  git_hash: str
  chromium_commit_position: str
  master: str
  v8_git_hash: str
  webrtc_git_hash: str


def get_gcs_prefix_path(
    build_properties: Dict[str, Any],
    builder_details: PerfBuilderDetails,
    benchmark_name: str,
    given_datetime: Optional[datetime.datetime],
    filename: Optional[str]) -> str:
  """Returns the gcs prefix path.

  Args:
    build_properties: The build properties.
    builder_details: The perf builder details.
    benchmark_name: The benchmark name.
    given_datetime: A datetime for deterministic testing.
    filename: The filename of the json file appends to the path.
  returns:
    The gcs prefix path.
  """
  now = given_datetime or datetime.datetime.now()  # Get the {yyyy/mm/dd}
  year = now.year
  month = now.month
  day = now.day
  candidates = [
      # ingest/yyyy/mm/dd/master/buildername/buildnumber/benchmarkname/filename
      "ingest",
      "{:04d}".format(year),
      "{:02d}".format(month),
      "{:02d}".format(day),
      builder_details.master,
      build_properties["buildername"],
      str(build_properties["buildnumber"]),
      benchmark_name,
  ]
  if filename:
    candidates.append(filename)

  gcs_prefix_path = r"/".join(candidates)
  return gcs_prefix_path


def perf_builder_details_from_build_properties(
    properties: Dict[str, Any],
    configuration_name: str,
    machine_group: str) -> PerfBuilderDetails:
  """Returns a PerfBuilderDetails from a given environment."""
  match = re.search(r"@\{#(\d+)\}", properties["got_revision_cp"])
  if match:
    revision = match.group(1)  # Return the captured group (the hash)
  else:
    revision = ""
  # TODO(crbug.com/318738818): Replace the url template with a more generic one
  # that works for chromium and chrome builders.
  result_url = "https://ci.chromium.org/ui/p/chrome/builders/ci/{}/{}".format(
      properties["buildername"], properties["buildnumber"])
  return PerfBuilderDetails(
      bot=configuration_name,
      builder_page=result_url,
      git_hash="CP:{}".format(revision) if revision else "",
      chromium_commit_position=properties["got_revision_cp"],
      master=machine_group,
      v8_git_hash=properties["got_v8_revision"],
      webrtc_git_hash=properties["got_webrtc_revision"],
  )


def links_from_builder_details(
    builder_details: PerfBuilderDetails,
    bot_ids: Set[str],
    os_versions: Set[str],
) -> Dict[str, str]:
  """Returns a dictionary of links from builder details."""
  links = collections.defaultdict(str)
  links[json_constants.BUILD_PAGE] = (builder_details.builder_page
                                      if builder_details else "")
  links[json_constants.OS_VERSION] = ""
  links[json_constants.BOT_IDS] = ""
  links[json_constants.CHROMIUM_COMMIT_POSITION] = (
      builder_details.chromium_commit_position if builder_details else "")
  links[json_constants.V8_GIT_HASH] = (builder_details.v8_git_hash
                                       if builder_details else "")
  links[json_constants.WEBRTC_GIT_HASH] = (builder_details.webrtc_git_hash
                                           if builder_details else "")
  links[json_constants.BOT_IDS] = ", ".join(
      str(bot_id) for bot_id in sorted(bot_ids))
  links[json_constants.OS_VERSION] = ", ".join(
      str(os_version) for os_version in sorted(os_versions))
  return links


def key_from_builder_details(builder_details: PerfBuilderDetails,
                             benchmark_key: Optional[str]) -> Dict[str, str]:
  """Returns a dictionary of key from builder details."""
  key = collections.defaultdict(str)
  key[json_constants.MASTER] = builder_details.master if builder_details else ""
  key[json_constants.BOT] = builder_details.bot if builder_details else ""
  key[json_constants.BENCHMARK] = ""
  key[json_constants.BENCHMARK] = benchmark_key if benchmark_key else ""
  return key


def is_empty(data: Optional[Dict[Any, Any]]) -> bool:
  if not data:
    return True
  if json_constants.RESULTS not in data or not data[json_constants.RESULTS]:
    return True
  return False


def _get_improvement_direction(unit: str) -> str:
  """Returns the improvement direction for a given unit."""
  return "down" if "smallerIsBetter" in unit else "up"


class JsonUtil:
  """Tools to convert result2 json to skia json."""

  def __init__(self, generate_synthetic_measurements: bool = False):
    self.generate_synthetic_measurements = generate_synthetic_measurements
    self._result2_jsons: List[Dict[str, Any]] = []

  def add(self, result2_json: List[Dict[str, str]]):
    """Adds a result2 json to the util."""
    self._result2_jsons.extend(result2_json)

  def _merge(
      self,
      builder_details: PerfBuilderDetails,
  ) -> Tuple[
      Mapping[List[Any], List[Any]],
      Mapping[str, str],
      Mapping[str, str],
  ]:
    """Merges the results."""
    benchmark_key = None
    bot_ids = set()
    os_versions = set()
    merged_results = collections.defaultdict(list)
    guid_to_values = collections.defaultdict(str)
    for item in self._result2_jsons:
      if (item.get("type") == json_constants.GENERIC_SET
          and json_constants.GUID in item and json_constants.VALUES in item):
        guid_to_values[item[json_constants.GUID]] = item[json_constants.VALUES]
      if json_constants.DIAGNOSTICS in item:
        test_name = item[json_constants.NAME]
        improvement_direction = _get_improvement_direction(
            item[json_constants.UNIT])
        if not isinstance(item[json_constants.DIAGNOSTICS], dict):
          raise ValueError("The diagnostics should be a dict, but it is %s" %
                           type(item[json_constants.DIAGNOSTICS]))
        stories = []
        story_tags = []
        for diagnostic_type, guid in item[json_constants.DIAGNOSTICS].items():
          # diagnostics_map[guid] = diagnostic_type
          if diagnostic_type == json_constants.BOT_ID:
            if guid in guid_to_values:
              bot_ids.update(guid_to_values[guid])
          elif diagnostic_type == json_constants.OS_DETAILED_VERSIONS:
            if guid in guid_to_values:
              os_versions.update(guid_to_values[guid])
          elif diagnostic_type == json_constants.BENCHMARKS:
            benchmark_key = str(guid_to_values[guid][0])
          elif diagnostic_type == json_constants.STORIES:
            stories.extend(guid_to_values[guid])
          elif diagnostic_type == json_constants.STORY_TAGS:
            story_tags.extend(guid_to_values[guid])
        subtest_1, subtest_2 = extract_subtest_from_stories_tags(
            stories, story_tags)

        try:
          if json_constants.SAMPLE_VALUES in item:
            if subtest_1 or subtest_2:
              merged_results[(
                  test_name,
                  item[json_constants.UNIT],
                  improvement_direction,
                  subtest_1,
                  subtest_2,
              )].extend(item["sampleValues"])
            else:
              merged_results[(test_name, item[json_constants.UNIT],
                              improvement_direction)].extend(
                                  item["sampleValues"])
          elif json_constants.SUMMARY_OPTIONS in item:
            if (json_constants.COUNT in item[json_constants.SUMMARY_OPTIONS]
                and item[json_constants.SUMMARY_OPTIONS][json_constants.COUNT]
                == "false"):
              continue
          else:
            # Some benchmarks don"t have sample values nor summary options.
            # For instance, "render_accessibility_locations".
            continue
        except KeyError as exc:
          raise ValueError(
              "The sampleValues should be in the item, but it is not there. %s"
              % item) from exc

    links = links_from_builder_details(builder_details, bot_ids, os_versions)
    key = key_from_builder_details(builder_details, benchmark_key)
    return merged_results, links, key

  def process(
      self, builder_details: PerfBuilderDetails, benchmark_name: str=""
      ) -> Dict[str, Any]:
    """Processes the result2 jsons and returns a skia json.

    Args:
      builder_details: The perf builder details.
      benchmark_name: The optional benchmark name, if present, replace the
        key[json_constants.BENCHMARK] with the given benchmark name.
    Returns:
      The skia json data.
    """
    output = {
        json_constants.VERSION: 1,
        json_constants.GIT_HASH: (builder_details.git_hash if builder_details
                                  else ""),
        json_constants.KEY: collections.defaultdict(str),
        json_constants.RESULTS: [],
    }

    # diagnostics_map = {}
    merged_results, links, key = self._merge(builder_details)
    logging.info("key has an original benchmark name: %s",
                 key[json_constants.BENCHMARK])
    if benchmark_name:
      # A few example is that "resource_sizes (TrichromeGoogle)" uses
      # "resource_sizes" in the result2 json, and "resource_sizes" is used
      # as the benchmark name. Replace it with the proper benchmark name
      # instead.
      key[json_constants.BENCHMARK] = benchmark_name
      logging.info("key has a new benchmark name: %s",
                   key[json_constants.BENCHMARK])

    output[json_constants.KEY] = key
    output[json_constants.LINKS] = links
    measurements = self.measurements_from_results(merged_results)

    output[json_constants.RESULTS] = measurements

    return output

  def _generate_synthetic_measurements(
      self,
      value_measurements: List[Tuple[str, float]],
      keys: Dict[str, str],
  ) -> List[Dict[str, Any]]:
    """Generates synthetic measurements."""
    synthetic_measurements = []
    for value, measurement in value_measurements:
      synthetic_measurements.append({
          json_constants.VALUE: value,
          json_constants.MEASUREMENT: measurement,
      })
    synthetic_result = {
        json_constants.MEASUREMENTS: {
            json_constants.STAT: synthetic_measurements
        },
        json_constants.KEY: {
            json_constants.IMPROVEMENT_DIRECTION: (
                keys[json_constants.IMPROVEMENT_DIRECTION]),
            json_constants.UNIT: keys[json_constants.UNIT],
            json_constants.TEST: keys[json_constants.TEST],
        },
    }
    if keys.get(json_constants.SUBTEST_1, ""):
      synthetic_result[json_constants.KEY][json_constants.SUBTEST_1] = (
          keys[json_constants.SUBTEST_1])
    if keys.get(json_constants.SUBTEST_2, ""):
      synthetic_result[json_constants.KEY][json_constants.SUBTEST_2] = (
          keys[json_constants.SUBTEST_2])
    return synthetic_result

  def measurements_from_results(
      self,
      data: Mapping[List[Any], List[Any]],
  ) -> List[Dict[str, Any]]:
    """Calculates the measurements for each test."""
    results = []
    if not data:
      return results
    for key, values in data.items():
      if len(key) == 3:
        test_name, unit, improvement_direction = key
        subtest_1, subtest_2 = None, None
      else:
        test_name, unit, improvement_direction, subtest_1, subtest_2 = key
      avg, std_err, count, max_val, min_val, sum_val = calculate_stats(values)
      measurements = [
          {
              json_constants.VALUE: json_constants.VALUE,
              json_constants.MEASUREMENT: avg,
          },
          {
              json_constants.VALUE: json_constants.STD_DEV,
              json_constants.MEASUREMENT: std_err,
          },
          {
              json_constants.VALUE: json_constants.COUNT,
              json_constants.MEASUREMENT: float(count),
          },
          {
              json_constants.VALUE: json_constants.MAX,
              json_constants.MEASUREMENT: max_val,
          },
          {
              json_constants.VALUE: json_constants.MIN,
              json_constants.MEASUREMENT: min_val
          },
          {
              json_constants.VALUE: json_constants.SUM,
              json_constants.MEASUREMENT: sum_val
          },
      ]
      result = {
          json_constants.MEASUREMENTS: {
              json_constants.STAT: measurements
          },
          json_constants.KEY: {
              json_constants.IMPROVEMENT_DIRECTION: improvement_direction,
              json_constants.UNIT: unit,
              json_constants.TEST: test_name,
          },
      }
      if subtest_1:
        result[json_constants.KEY][json_constants.SUBTEST_1] = subtest_1
      if subtest_2:
        result[json_constants.KEY][json_constants.SUBTEST_2] = subtest_2
      results.append(result)
      # Generate a synthetic measurement that ends with "_avg", "_min", "_max",
      # and "_sum" to support the data parity with the chromeperf.
      if self.generate_synthetic_measurements:
        synthetic_result_avg = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.AVERAGE, avg),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: improvement_direction,
                json_constants.UNIT: unit,
                json_constants.TEST: test_name + "_avg",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        synthetic_result_min = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.MIN, min_val),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: improvement_direction,
                json_constants.UNIT: unit,
                json_constants.TEST: test_name + "_min",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        synthetic_result_max = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.MAX, max_val),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: improvement_direction,
                json_constants.UNIT: unit,
                json_constants.TEST: test_name + "_max",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        synthetic_result_sum = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.SUM, sum_val),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: improvement_direction,
                json_constants.UNIT: unit,
                json_constants.TEST: test_name + "_sum",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        synthetic_result_count = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.COUNT, count),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: "up",
                json_constants.UNIT: "unitless_biggerIsBetter",
                json_constants.TEST: test_name + "_count",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        synthetic_result_std = self._generate_synthetic_measurements(
            value_measurements=[
                (json_constants.STD_DEV, std_err),
            ],
            keys={
                json_constants.IMPROVEMENT_DIRECTION: "down",
                json_constants.UNIT: unit,
                json_constants.TEST: test_name + "_std",
                json_constants.SUBTEST_1: subtest_1,
                json_constants.SUBTEST_2: subtest_2,
            },
        )
        results.append(synthetic_result_avg)
        results.append(synthetic_result_min)
        results.append(synthetic_result_max)
        results.append(synthetic_result_sum)
        results.append(synthetic_result_count)
        results.append(synthetic_result_std)
    return results
