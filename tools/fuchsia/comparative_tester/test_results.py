# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from typing import Any, Dict, List, Tuple, Optional


def UnitStringIsValid(unit: str) -> bool:
  """Checks to make sure that a given string is in fact a recognized unit used
      by the chromium perftests to report results.

  Args:
    unit (str): The unit string to be checked.

  Returns:
    bool: Whether or not it is a unit.
  """
  accepted_units = [
      "us/hop", "us/task", "ns/sample", "ms", "s", "count", "KB", "MB/s", "us"
  ]
  return unit in accepted_units


class ResultLine(object):
  """ResultLine objects are each an individual line of output, complete with a
  unit, measurement, and descriptive component.
  """

  def __init__(self, desc: str, measure: float, unit: str) -> None:
    self.desc = desc
    self.meas = measure
    self.unit = unit

  def ToJsonDict(self) -> Dict[str, Any]:
    """Converts a ResultLine into a JSON-serializable dictionary object.

    Returns:
      Dict[str, Any]: A mapping of strings that will appear in the output JSON
          object to their respective values.
    """

    return {
        "description": self.desc,
        "measurement": self.meas,
        "unit": self.unit,
    }


def ReadResultLineFromJson(dct: Dict[str, Any]) -> ResultLine:
  """Converts a JSON dictionary object into a ResultLine.

  Args:
    dct (Dict[str, Any]): The JSON object to be parsed as a ResultLine. MUST
        contain the strings 'description', 'measurement', and 'unit'.

  Raises:
    KeyError: If the passed in dictionary does not contain the three required
        strings that a serialized ResultLine must contain.

  Returns:
    ResultLine: A ResultLine object reconstituted from the JSON dictionary.
  """
  return ResultLine(dct["description"], float(dct["measurement"]), dct["unit"])


def ResultLineFromStdout(line: str) -> Optional[ResultLine]:
  """Takes a line of stdout data and attempts to parse it into a ResultLine.

  Args:
    line (str): The stdout line to be converted

  Returns:
    Optional[ResultLine]: The output is Optional, because the line may be noise,
        or in some way incorrectly formatted and unparseable.
  """

  if "pkgsvr" in line:
    return None # Filters pkgsrv noise from Fuchsia output.
  chunks = line.split()
  # There should be 1 chunk for the measure, 1 for the unit, and at least one
  # for the line description, so at least 3 total
  if len(chunks) < 3:
    logging.warning("The line {} contains too few space-separated pieces to be "
                    "parsed as a ResultLine".format(line))
    return None
  unit = chunks[-1]
  if not UnitStringIsValid(unit):
    logging.warning("The unit string parsed from {} was {}, which was not "
                    "expected".format(line, unit))
    return None
  try:
    measure = float(chunks[-2])
    desc = " ".join(chunks[:-2])
    return ResultLine(desc, measure, unit)
  except ValueError as e:
    logging.warning("The chunk {} could not be parsed as a valid measurement "
                    "because of {}".format(chunks[-2], str(e)))
    return None


class TestResult(object):
  """TestResult objects comprise the smallest unit of a GTest target, and
  contain the name of the individual test run, and the time that the test took
  to run."""

  def __init__(self, name: str, time: float, lines: List[ResultLine]) -> None:
    self.name = name
    self.time = time
    self.lines = lines

  def ToJsonDict(self) -> Dict[str, Any]:
    """Converts a TestResult object to a JSON-serializable dictionary.

    Returns:
      Dict[str, Any]: The output dictionary object that can be directly
          serialized to JSON.
    """
    return {
        "name": self.name,
        "time_in_ms": self.time,
        "lines": [line.ToJsonDict() for line in self.lines]
    }


def ReadTestFromJson(obj_dict: Dict[str, Any]) -> TestResult:
  """Reconstitutes a TestResult read from a JSON file back into a TestResult
  object.

  Args:
    obj_dict (Dict[str, Any]): The JSON object as read from an output JSON file.

  Returns:
    TestResult: The reconstituted TestResult object.
  """
  name = obj_dict["name"]
  time = obj_dict["time_in_ms"]
  lines = [ReadResultLineFromJson(line) for line in obj_dict["lines"]]
  return TestResult(name, time, lines)


def ExtractTestInfo(line: str) -> Tuple[str, float]:
  """Deconstructs a line starting with OK, stating that the test finished
  successfully, and isolates the timing measurement as well as a descriptive
  string for the test

  Args:
    line (str): The line of output to attempt to destructure into name and time.

  Raises:
    Exception: In the event that it couldn't split on '(', because then it
        find the necessary timing measurement.
    Exception: in the event that it cannot find the ')' character in the output,
        because then it isn't capable of isolating the timing measurement.

  Returns:
    Tuple[str, float]: A tuple of the test name, and the amount of time it took
        to run.
  """

  # Trim off the [       OK ] part of the line
  trimmed = line.lstrip("[       OK ]").strip()
  try:
    test_name, rest = trimmed.split("(")  # Isolate the measurement
  except Exception as e:
    err_text = "Could not extract the case name from {} because of error {}"\
               .format(trimmed, str(e))
    raise Exception(err_text)
  try:
    measure, _ = rest.split(")", 1)[0].split()
  except Exception as e:
    err_text = "Could not extract measure and units from {}\
                because of error {}".format(rest, str(e))
    raise Exception(err_text)
  return test_name.strip(), float(measure)


def TaggedTestFromLines(lines: List[str]) -> TestResult:
  """Takes a chunk of lines gathered together from the stdout of a test process
  and collects it all into a single test result, including the set of
  ResultLines inside of the TestResult.

  Args:
    lines (List[str]): The stdout lines to be parsed into a single test result

  Returns:
    TestResult: The final parsed TestResult from the input
  """

  test_name, time = ExtractTestInfo(lines[-1])
  res_lines = []
  for line in lines[:-1]:
    res_line = ResultLineFromStdout(line)
    if res_line:
      res_lines.append(res_line)
    else:
      logging.warning("Couldn't parse line {} into a ResultLine".format(line))
  return TestResult(test_name, time, res_lines)


class TargetResult(object):
  """TargetResult objects contain the entire set of TestSuite objects that are
  invoked by a single test target, such as base:base_perftests and the like.
  They also include the name of the target, and the time it took the target to
  run.
  """

  def __init__(self, name: str, tests: List[TestResult]) -> None:
    self.name = name
    self.tests = tests

  def ToJsonDict(self) -> Dict[str, Any]:
    """Converts a TargetResult to a JSON-serializable dict.

    Returns:
      Dict[str, Any]: The TargetResult in JSON-serializable form.
    """
    return {
        "name": self.name,
        "tests": [test.ToJsonDict() for test in self.tests]
    }

  def WriteToJson(self, path: str) -> None:
    """Writes this TargetResult object as a JSON file at the given file path.

    Args:
      path (str): The location to place the serialized version of this object.

    Returns:
      None: It'd return IO (), but this Python.
    """
    with open(path, "w") as outfile:
      json.dump(self.ToJsonDict(), outfile, indent=2)


def ReadTargetFromJson(path: str) -> Optional[TargetResult]:
  """Takes a file path and attempts to parse a TargetResult from it.

  Args:
    path (str): The location of the JSON-serialized TargetResult to read.

  Returns:
    Optional[TargetResult]: Again, technically should be wrapped in an IO,
        but Python.
  """
  with open(path, "r") as json_file:
    dct = json.load(json_file)
    return TargetResult(
        dct["name"], [ReadTestFromJson(test_dct) for test_dct in dct["tests"]])


def TargetResultFromStdout(lines: List[str], name: str) -> TargetResult:
  """TargetResultFromStdout attempts to associate GTest names to the lines of
  output that they produce. Example input looks something like the following:

  [ RUN      ] TestNameFoo
  INFO measurement units
  ...
  [       OK ] TestNameFoo (measurement units)
  ...

  Unfortunately, Because the results of output from perftest targets is not
  necessarily consistent between test targets, this makes a best-effort to parse
  as much information from them as possible.

  Args:
    lines (List[str]): The entire list of lines from the standard output to be
        parsed.
    name (str): The name of the Target that generated the output. Necessary to
        be able to give the TargetResult a meaningful name.

  Returns:
    TargetResult: The TargetResult object parsed from the input lines.
  """

  test_line_lists = []  # type: List[List[str]]
  test_line_accum = []  # type: List[str]
  read_lines = False
  for line in lines:
    # We're starting a test suite
    if line.startswith("[ RUN      ]"):
      read_lines = True
      # We have a prior suite that needs to be added
      if len(test_line_accum) > 0:
        test_line_lists.append(test_line_accum)
        test_line_accum = []
    elif read_lines:
      # We don't actually care about the data in the RUN line, just its
      # presence. the OK line contains the same info, as well as the total test
      # run time
      test_line_accum.append(line)
      if line.startswith("[       OK ]"):
        read_lines = False

  test_cases = [
      TaggedTestFromLines(test_lines) for test_lines in test_line_lists
  ]
  return TargetResult(name, test_cases)
