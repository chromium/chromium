#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""generate_perf_report.py is to be used after comparative_tester.py has been
executed and written some test data into the location specified by
target_spec.py. It writes to results_dir and reads all present test info from
raw_data_dir. Using this script should just be a matter of invoking it from
chromium/src while raw test data exists in raw_data_dir."""

import json
import logging
import math
import os
import sys
from typing import List, Dict, Set, Tuple, Optional, Any, TypeVar, Callable

import target_spec
from test_results import (TargetResult, ReadTargetFromJson, TestResult,
                          ResultLine)


class LineStats(object):

  def __init__(self, desc: str, unit: str, time_avg: float, time_dev: float,
               cv: float, samples: int) -> None:
    """A corpus of stats about a particular line from a given test's output.

    Args:
      desc (str): Descriptive text of the line in question.
      unit (str): The units of measure that the line's result is in.
      time_avg (float): The average measurement.
      time_dev (float): The standard deviation of the measurement.
      cv (float): The coefficient of variance of the measure.
      samples (int): The number of samples that went into making this object.
    """

    self.desc = desc
    self.time_avg = time_avg
    self.time_dev = time_dev
    self.cv = cv
    self.unit = unit
    self.sample_num = samples

  def ToString(self) -> str:
    """Converts the line to a human-readable string."""
    if self.sample_num > 1:
      return "{}: {:.5f} σ={:.5f} {} with n={} cv={}".format(
          self.desc, self.time_avg, self.time_dev, self.unit, self.sample_num,
          self.cv)
    else:
      return "{}: {:.5f} with only one sample".format(self.desc, self.time_avg)


def LineFromList(lines: List[ResultLine]) -> LineStats:
  """Takes a list of ResultLines and generates statistics for them.

  Args:
    lines (List[ResultLine]): The list of lines to generate stats for.

  Returns:
    LineStats: the representation of statistical data for the lines.
  """

  desc = lines[0].desc
  unit = lines[0].unit
  times = [line.meas for line in lines]
  avg, dev, cv = GenStats(times)
  return LineStats(desc, unit, avg, dev, cv, len(lines))


class TestStats(object):

  def __init__(self, name: str, time_avg: float, time_dev: float, cv: float,
               samples: int, lines: List[LineStats]) -> None:
    """Represents a summary of relevant statistics for a list of tests.

    Args:
      name (str): The name of the test whose runs are being averaged.
      time_avg (float): The average time to execute the test.
      time_dev (float): The standard deviation in the mean.
      cv (float): The coefficient of variance of the population.
      samples (int): The number of samples in the population
      lines (List[LineStats]): The averaged list of all the lines of output that
          comprises this test.
    """
    self.name = name
    self.time_avg = time_avg
    self.time_dev = time_dev
    self.cv = cv
    self.sample_num = samples
    self.lines = lines

  def ToLines(self) -> List[str]:
    """The stats of this test, as well as its constituent LineStats, in a human-
    readable format.

    Returns:
      List[str]: The human-readable list of lines.
    """

    lines = []
    if self.sample_num > 1:
      lines.append("{}: {:.5f} σ={:.5f}ms with n={} cv={}".format(
          self.name, self.time_avg, self.time_dev, self.sample_num, self.cv))
    else:
      lines.append("{}: {:.5f} with only one sample".format(
          self.name, self.time_avg))
    for line in self.lines:
      lines.append("  {}".format(line.ToString()))
    return lines


def TestFromList(tests: List[TestResult]) -> TestStats:
  """Coalesces a list of TestResults into a single TestStats object.

  Args:
    tests (List[TestResult]): The input sample of the tests.

  Returns:
    TestStats: A representation of the statistics of the tests.
  """

  name = tests[0].name
  avg, dev, cv = GenStats([test.time for test in tests])
  lines = {}  # type: Dict[str, List[ResultLine]]
  for test in tests:
    assert test.name == name
    for line in test.lines:
      if not line.desc in lines:
        lines[line.desc] = [line]
      else:
        lines[line.desc].append(line)
  test_lines = []
  for _, line_list in lines.items():
    stat_line = LineFromList(line_list)
    if stat_line:
      test_lines.append(stat_line)
  return TestStats(name, avg, dev, cv, len(tests), test_lines)


class TargetStats(object):

  def __init__(self, name: str, samples: int, tests: List[TestStats]) -> None:
    """A representation of the actual target that was built and run on the
    platforms multiple times to generate statistical data.

    Args:
      name (str): The name of the target that was built and run.
      samples (int): The number of times the tests were run.
      tests (List[TestStats]): The statistics of tests included in the target.
    """

    self.name = name
    self.sample_num = samples
    self.tests = tests

  def ToLines(self) -> List[str]:
    """Converts the entire target into a list of lines in human-readable format.

    Returns:
      List[str]: The human-readable test lines.
    """
    lines = []
    if self.sample_num > 1:
      lines.append("{}: ".format(self.name))
    else:
      lines.append("{}: with only one sample".format(self.name))
    for test in self.tests:
      for line in test.ToLines():
        lines.append("  {}".format(line))
    return lines

  def __format__(self, format_spec):
    return "\n".join(self.ToLines())


def TargetFromList(results: List[TargetResult]) -> TargetStats:
  """Coalesces a list of TargetResults into a single collection of stats.

  Args:
    results (List[TargetResult]): The sampling of target executions to generate
        stats for.

  Returns:
    TargetStats: The body of stats for the sample given.
  """

  name = results[0].name
  sample_num = len(results)
  tests = {}  # type: Dict[str, List[TestResult]]
  for result in results:
    assert result.name == name
    # This groups tests by name so that they can be considered independently,
    # so that in the event tests flake out, their average times can
    # still be accurately calculated
    for test in result.tests:
      if not test.name in tests.keys():
        tests[test.name] = [test]
      tests[test.name].append(test)
  test_stats = [TestFromList(test_list) for _, test_list in tests.items()]
  return TargetStats(name, sample_num, test_stats)


def GenStats(corpus: List[float]) -> Tuple[float, float, float]:
  """Generates statistics from a list of values

  Args:
    corpus (List[float]): The set of data to generate statistics for.

  Returns:
    Tuple[float, float, float]: The mean, standard deviation, and coefficient of
        variation for the given sample data.
  """
  avg = sum(corpus) / len(corpus)
  adjusted_sum = 0.0
  for item in corpus:
    adjusted = item - avg
    adjusted_sum += adjusted * adjusted

  dev = math.sqrt(adjusted_sum / len(corpus))
  cv = dev / avg
  return avg, dev, cv


def DirectoryStats(directory: str) -> List[TargetStats]:
  """Takes a path to directory, and uses JSON files in that directory to compile
  a list of statistical objects for each independent test target it can detect
  in the directory.

  Args:
    directory (str): The directory to scan for relevant JSONs

  Returns:
    List[TargetStats]: Each element in this list is one target, averaged up over
        all of its executions.
  """
  resultMap = {}  # type: Dict[str, List[TargetResult]]
  for file in os.listdir(directory):
    results = ReadTargetFromJson("{}/{}".format(directory, file))
    if not results.name in resultMap.keys():
      resultMap[results.name] = [results]
    else:
      resultMap[results.name].append(results)

  targets = []
  for _, resultList in resultMap.items():
    targets.append(TargetFromList(resultList))
  return targets


def CompareTargets(linux: TargetStats, fuchsia: TargetStats) -> Dict[str, Any]:
  """Compare takes a corpus of statistics from both Fuchsia and Linux, and then
  lines up the values, compares them to each other, and writes them into a
  dictionary that can be JSONified.
  """
  if linux and fuchsia:
    assert linux.name == fuchsia.name
    paired_tests = ZipListsByPredicate(linux.tests, fuchsia.tests,
                                     lambda test: test.name)
    paired_tests = MapDictValues(paired_tests, CompareTests)
    return {"name": linux.name, "tests": paired_tests}
  else:
    # One of them has to be non-null, by the way ZipListsByPredicate functions
    assert linux or fuchsia
    if linux:
      logging.error("Fuchsia was missing test target {}".format(linux.name))
    else:
      logging.error("Linux was missing test target {}".format(fuchsia.name))
    return None


def CompareTests(linux: TestStats, fuchsia: TestStats) -> Dict[str, Any]:
  """As CompareTargets, but at the test level"""
  if not linux and not fuchsia:
    logging.error("Two null TestStats objects were passed to CompareTests.")
    return {}

  if not linux or not fuchsia:
    if linux:
      name = linux.name
      failing_os = "Fuchsia"
    else:
      name = fuchsia.name
      failing_os = "Linux"
    logging.error("%s failed to produce output for the test %s",
                  failing_os, name)
    return {}

  assert linux.name == fuchsia.name
  paired_lines = ZipListsByPredicate(linux.lines, fuchsia.lines,
                                     lambda line: line.desc)
  paired_lines = MapDictValues(paired_lines, CompareLines)
  result = {"lines": paired_lines, "unit": "ms"}  # type: Dict[str, Any]

  if linux:
    result["name"] = linux.name
    result["linux_avg"] = linux.time_avg
    result["linux_dev"] = linux.time_dev
    result["linux_cv"] = linux.cv

  if fuchsia == None:
    logging.warning("Fuchsia is missing test case {}".format(linux.name))
  else:
    result["name"] = fuchsia.name
    result["fuchsia_avg"] = fuchsia.time_avg
    result["fuchsia_dev"] = fuchsia.time_dev
    result["fuchsia_cv"] = fuchsia.cv
  return result


def CompareLines(linux: LineStats, fuchsia: LineStats) -> Dict[str, Any]:
  """CompareLines wraps two LineStats objects up as a JSON-dumpable dict.
  It also logs a warning every time a line is given which can't be matched up.
  If both lines passed are None, or their units or descriptions are not the same
  (which should never happen) this function fails.
  """
  if linux != None and fuchsia != None:
    assert linux.desc == fuchsia.desc
    assert linux.unit == fuchsia.unit
  assert linux != None or fuchsia != None

  # ref_test is because we don't actually care which test we get the values
  # from, as long as we get values for the name and description
  ref_test = linux if linux else fuchsia
  result = {"desc": ref_test.desc, "unit": ref_test.unit}

  if fuchsia == None:
    logging.warning("Fuchsia is missing test line {}".format(linux.desc))
  else:
    result["fuchsia_avg"] = fuchsia.time_avg
    result["fuchsia_dev"] = fuchsia.time_dev
    result["fuchsia_cv"] = fuchsia.cv

  if linux:
    result["linux_avg"] = linux.time_avg
    result["linux_dev"] = linux.time_dev
    result["linux_cv"] = linux.cv

  return result


T = TypeVar("T")
R = TypeVar("R")


def ZipListsByPredicate(left: List[T], right: List[T],
                        pred: Callable[[T], R]) -> Dict[R, Tuple[T, T]]:
  """This function takes two lists, and a predicate. The predicate is applied to
  the values in both lists to obtain a keying value from them. Each item is then
  inserted into the returned dictionary using the obtained key. The predicate
  should not map multiple values from one list to the same key.
  """
  paired_items = {}  # type: Dict [R, Tuple[T, T]]
  for item in left:
    key = pred(item)
    # the first list shouldn't cause any key collisions
    assert key not in paired_items.keys()
    paired_items[key] = item, None

  for item in right:
    key = pred(item)
    if key in paired_items.keys():
      # elem 1 of the tuple is always None if the key exists in the map
      prev, _ = paired_items[key]
      paired_items[key] = prev, item
    else:
      paired_items[key] = None, item

  return paired_items


U = TypeVar("U")
V = TypeVar("V")


def MapDictValues(dct: Dict[T, Tuple[R, U]],
                  predicate: Callable[[R, U], V]) -> Dict[T, V]:
  """This function applies the predicate to all the values in the dictionary,
  returning a new dictionary with the new values.
  """
  out_dict = {}
  for key, val in dct.items():
    out_dict[key] = predicate(*val)
  return out_dict


def main():
  linux_avgs = DirectoryStats(target_spec.raw_linux_dir)
  fuchsia_avgs = DirectoryStats(target_spec.raw_fuchsia_dir)
  paired_targets = ZipListsByPredicate(linux_avgs, fuchsia_avgs,
                                       lambda target: target.name)
  for name, targets in paired_targets.items():
    comparison_dict = CompareTargets(*targets)
    if comparison_dict:
      with open("{}/{}.json".format(target_spec.results_dir, name),
                "w") as outfile:
        json.dump(comparison_dict, outfile, indent=2)


if __name__ == "__main__":
  sys.exit(main())
