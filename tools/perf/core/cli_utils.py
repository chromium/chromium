# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import datetime
import logging

from core import gsutil


def VerboseLevel(count):
  if count == 0:
    return logging.WARNING
  if count == 1:
    return logging.INFO
  return logging.DEBUG


def ConfigureLogging(verbose_count):
  logging.basicConfig(level=VerboseLevel(verbose_count))


def OpenWrite(filepath):
  """Open file for writing, optionally supporting cloud storage paths."""
  if filepath.startswith('gs://'):
    return gsutil.OpenWrite(filepath)
  return open(filepath, 'w')


def DaysAgoToTimestamp(num_days):
  """Return an ISO formatted timestamp for a number of days ago."""
  timestamp = datetime.datetime.utcnow() - datetime.timedelta(days=num_days)
  return timestamp.isoformat()


def MergeIndexRanges(section_list):
  """Given a list of (begin, end) ranges, return the merged ranges.

    Args:
      range_list: a list of index ranges as (begin, end)
    Return:
      a list of merged index ranges.
  """
  actions = []
  for section in section_list:
    if section[0] >= section[1]:
      raise ValueError('Invalid range: (%d, %d)' % (section[0], section[1]))
    actions.append((section[0], 1))
    actions.append((section[1], -1))

  actions.sort(key=lambda x: (x[0], -x[1]))

  merged_indexes = []
  status = 0
  start = -1
  for action in actions:
    if start == -1:
      start = action[0]
    status += action[1]
    if status == 0:
      merged_indexes.append((start, action[0]))
      start = -1

  return merged_indexes
