# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import datetime
import logging

from core import gsutil


def VerboseLevel(count):
  if count == 0:
    return logging.WARNING
  elif count == 1:
    return logging.INFO
  else:
    return logging.DEBUG


def ConfigureLogging(verbose_count):
  logging.basicConfig(level=VerboseLevel(verbose_count))


def OpenWrite(filepath):
  """Open file for writing, optionally supporting cloud storage paths."""
  if filepath.startswith('gs://'):
    return gsutil.OpenWrite(filepath)
  else:
    return open(filepath, 'w')


def DaysAgoToTimestamp(num_days):
  """Return an ISO formatted timestamp for a number of days ago."""
  timestamp = datetime.datetime.utcnow() - datetime.timedelta(days=num_days)
  return timestamp.isoformat()
