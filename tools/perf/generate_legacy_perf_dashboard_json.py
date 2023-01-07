#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generates legacy perf dashboard json from non-telemetry based perf tests.
Taken from chromium/build/scripts/slave/performance_log_processory.py
(https://goo.gl/03SQRk)
"""

import collections
import json
import math
import logging
import re


class LegacyResultsProcessor(object):
  """Class for any log processor expecting standard data to be graphed.

  The log will be parsed looking for any lines of the forms:
    <*>RESULT <graph_name>: <trace_name>= <value> <units>
  or
    <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...] <units>
  or
    <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>

  For example,
    *RESULT vm_final_browser: OneTab= 8488 kb
    RESULT startup: ref= [167.00,148.00,146.00,142.00] ms
    RESULT TabCapturePerformance_foo: Capture= {30.7, 1.45} ms

  The leading * is optional; it indicates that the data from that line should
  be considered "important", which may mean for example that it's graphed by
  default.

  If multiple values are given in [], their mean and (sample) standard
  deviation will be written; if only one value is given, that will be written.
  A trailing comma is permitted in the list of values.

  NOTE: All lines except for RESULT lines are ignored, including the Avg and
  Stddev lines output by Telemetry!

  Any of the <fields> except <value> may be empty, in which case the
  not-terribly-useful defaults will be used. The <graph_name> and <trace_name>
  should not contain any spaces, colons (:) nor equals-signs (=). Furthermore,
  the <trace_name> will be used on the waterfall display, so it should be kept
  short. If the trace_name ends with '_ref', it will be interpreted as a
  reference value, and shown alongside the corresponding main value on the
  waterfall.

  Semantic note: The terms graph and chart are used interchangeably here.
  """

  RESULTS_REGEX = re.compile(r'(?P<IMPORTANT>\*)?RESULT '
                             r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                             r'(?P<VALUE>[\{\[]?[-\d\., ]+[\}\]]?)('
                             r' ?(?P<UNITS>.+))?')
  # TODO(eyaich): Determine if this format is still used by any perf tests
  HISTOGRAM_REGEX = re.compile(r'(?P<IMPORTANT>\*)?HISTOGRAM '
                               r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                               r'(?P<VALUE_JSON>{.*})(?P<UNITS>.+)?')

  def __init__(self):
    # A dict of Graph objects, by name.
    self._graphs = {}
    # A dict mapping output file names to lists of lines in a file.
    self._output = {}
    self._percentiles = [.1, .25, .5, .75, .90, .95, .99]

  class Trace(object):
    """Encapsulates data for one trace. Here, this means one point."""

    def __init__(self):
      self.important = False
      self.values = []
      self.mean = 0.0
      self.stddev = 0.0

    def __str__(self):
      result = _FormatHumanReadable(self.mean)
      if self.stddev:
        result += '+/-%s' % _FormatHumanReadable(self.stddev)
      return result

  class Graph(object):
    """Encapsulates a set of points that should appear on the same graph."""

    def __init__(self):
      self.units = None
      self.traces = {}

    def IsImportant(self):
      """A graph is considered important if any of its traces is important."""
      for trace in self.traces.values():
        if trace.important:
          return True
      return False

    def BuildTracesDict(self):
      """Returns a dictionary mapping trace names to [value, stddev]."""
      traces_dict = {}
      for name, trace in self.traces.items():
        traces_dict[name] = [str(trace.mean), str(trace.stddev)]
      return traces_dict


  def GenerateJsonResults(self, filename):
    # Iterate through the file and process each output line
    with open(filename) as f:
      for line in f.readlines():
        self.ProcessLine(line)
    # After all results have been seen, generate the graph json data
    return self.GenerateGraphJson()


  def _PrependLog(self, filename, data):
    """Prepends some data to an output file."""
    self._output[filename] = data + self._output.get(filename, [])


  def ProcessLine(self, line):
    """Processes one result line, and updates the state accordingly."""
    results_match = self.RESULTS_REGEX.search(line)
    histogram_match = self.HISTOGRAM_REGEX.search(line)
    if results_match:
      self._ProcessResultLine(results_match)
    elif histogram_match:
      raise Exception("Error: Histogram results parsing not supported yet")


  def _ProcessResultLine(self, line_match):
    """Processes a line that matches the standard RESULT line format.

    Args:
      line_match: A MatchObject as returned by re.search.
    """
    match_dict = line_match.groupdict()
    graph_name = match_dict['GRAPH'].strip()
    trace_name = match_dict['TRACE'].strip()

    graph = self._graphs.get(graph_name, self.Graph())
    graph.units = (match_dict['UNITS'] or '').strip()
    trace = graph.traces.get(trace_name, self.Trace())
    value = match_dict['VALUE']
    trace.important = match_dict['IMPORTANT'] or False

    # Compute the mean and standard deviation for a list or a histogram,
    # or the numerical value of a scalar value.
    if value.startswith('['):
      try:
        value_list = [float(x) for x in value.strip('[],').split(',')]
      except ValueError:
        # Report, but ignore, corrupted data lines. (Lines that are so badly
        # broken that they don't even match the RESULTS_REGEX won't be
        # detected.)
        logging.warning("Bad test output: '%s'" % value.strip())
        return
      trace.values += value_list
      trace.mean, trace.stddev, filedata = self._CalculateStatistics(
        trace.values, trace_name)
      assert filedata is not None
      for filename in filedata:
        self._PrependLog(filename, filedata[filename])
    elif value.startswith('{'):
      stripped = value.strip('{},')
      try:
        trace.mean, trace.stddev = [float(x) for x in stripped.split(',')]
      except ValueError:
        logging.warning("Bad test output: '%s'" % value.strip())
        return
    else:
      try:
        trace.values.append(float(value))
        trace.mean, trace.stddev, filedata = self._CalculateStatistics(
          trace.values, trace_name)
        assert filedata is not None
        for filename in filedata:
          self._PrependLog(filename, filedata[filename])
      except ValueError:
        logging.warning("Bad test output: '%s'" % value.strip())
        return

    graph.traces[trace_name] = trace
    self._graphs[graph_name] = graph


  def GenerateGraphJson(self):
    """Writes graph json for each graph seen.
    """
    charts = {}
    for graph_name, graph in self._graphs.items():
      traces = graph.BuildTracesDict()

      # Traces should contain exactly two elements: [mean, stddev].
      for _, trace in traces.items():
        assert len(trace) == 2

      graph_dict = collections.OrderedDict([
        ('traces', traces),
        ('units', str(graph.units)),
      ])

      # Include a sorted list of important trace names if there are any.
      important = [t for t in graph.traces.keys() if graph.traces[t].important]
      if important:
        graph_dict['important'] = sorted(important)

      charts[graph_name] = graph_dict
    return json.dumps(charts)


  # _CalculateStatistics needs to be a member function.
  # pylint: disable=R0201
  # Unused argument value_list.
  # pylint: disable=W0613
  def _CalculateStatistics(self, value_list, trace_name):
    """Returns a tuple with some statistics based on the given value list.

    This method may be overridden by subclasses wanting a different standard
    deviation calcuation (or some other sort of error value entirely).

    Args:
      value_list: the list of values to use in the calculation
      trace_name: the trace that produced the data (not used in the base
          implementation, but subclasses may use it)

    Returns:
      A 3-tuple - mean, standard deviation, and a dict which is either
          empty or contains information about some file contents.
    """
    n = len(value_list)
    if n == 0:
      return 0.0, 0.0, {}
    mean = float(sum(value_list)) / n
    variance = sum([(element - mean)**2 for element in value_list]) / n
    stddev = math.sqrt(variance)

    return mean, stddev, {}


def _FormatHumanReadable(number):
  """Formats a float into three significant figures, using metric suffixes.

  Only m, k, and M prefixes (for 1/1000, 1000, and 1,000,000) are used.
  Examples:
    0.0387    => 38.7m
    1.1234    => 1.12
    10866     => 10.8k
    682851200 => 683M
  """
  metric_prefixes = {-3: 'm', 0: '', 3: 'k', 6: 'M'}
  scientific = '%.2e' % float(number)     # 6.83e+005
  e_idx = scientific.find('e')            # 4, or 5 if negative
  digits = float(scientific[:e_idx])      # 6.83
  exponent = int(scientific[e_idx + 1:])  # int('+005') = 5
  while exponent % 3:
    digits *= 10
    exponent -= 1
  while exponent > 6:
    digits *= 10
    exponent -= 1
  while exponent < -3:
    digits /= 10
    exponent += 1
  if digits >= 100:
    # Don't append a meaningless '.0' to an integer number.
    digits = int(digits)
  # Exponent is now divisible by 3, between -3 and 6 inclusive: (-3, 0, 3, 6).
  return '%s%s' % (digits, metric_prefixes[exponent])
