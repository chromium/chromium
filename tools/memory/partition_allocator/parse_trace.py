# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parses traces into Python objects.

Takes a trace from chrome://tracing and returns a Python dict containing the
result. The functions in the file are not very useful on their own, but are
intended as helpers for the other scripts in this directory.
"""

import gzip
import json


def LoadTrace(filename: str) -> dict:
  """Loads a JSON trace, gzipped or not.

  Args:
    filename: Filename, gzipped or not.

  Returns:
    A dictionary with the trace content.
  """
  try:
    f = None
    if filename.endswith('.gz'):
      f = gzip.open(filename, 'r')
    else:
      f = open(filename, 'r')
    return json.load(f)
  finally:
    if f is not None:
      f.close()


def GetAllocatorDumps(trace: dict) -> list:
  """Takes in a trace, and returns the parts of it related to the allocators.

  Args:
    trace: Trace content as returned by LoadTrace.

  Returns:
    The parts of the trace related to allocator metrics.
  """
  events = trace['traceEvents']
  memory_infra_events = [
      e for e in events if e['cat'] == 'disabled-by-default-memory-infra'
  ]
  dumps = [
      e for e in memory_infra_events
      if e['name'] == 'periodic_interval' and e['args']['dumps']
      ['level_of_detail'] == 'detailed' and 'allocators' in e['args']['dumps']
  ]
  return dumps


def ProcessNamesAndLabels(trace: dict) -> (dict, dict):
  """Get mappings of pid to name and pid to label.

  Args:
    trace: Trace content as returned by LoadTrace.

  Returns:
    A tuple containing two dicts. The first maps pids to names, the second maps
    pids to labels.
  """
  # Process names and labels.
  pid_to_name = {}
  pid_to_labels = {}

  metadata_events = [
      e for e in trace['traceEvents'] if e['cat'] == '__metadata'
  ]

  process_name_events = [
      e for e in metadata_events if e['name'] == 'process_name'
  ]
  for e in process_name_events:
    pid_to_name[e['pid']] = e['args']['name']

  process_labels_events = [
      e for e in metadata_events if e['name'] == 'process_labels'
  ]
  for e in process_labels_events:
    pid_to_labels[e['pid']] = e['args']['labels']

  return pid_to_name, pid_to_labels
