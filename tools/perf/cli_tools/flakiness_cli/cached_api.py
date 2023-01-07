# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import hashlib

from cli_tools.flakiness_cli import api
from cli_tools.flakiness_cli import frames


def GetBuilders():
  """Get the builders data frame and keep a cached copy."""
  def make_frame():
    data = api.GetBuilders()
    return frames.BuildersDataFrame(data)

  return frames.GetWithCache(
      'builders.pkl', make_frame, expires_after=datetime.timedelta(hours=12))


def GetTestResults(master, builder, test_type):
  """Get a test results data frame and keep a cached copy."""
  def make_frame():
    data = api.GetTestResults(master, builder, test_type)
    return frames.TestResultsDataFrame(data)

  basename = hashlib.md5('/'.join([master, builder, test_type])).hexdigest()
  return frames.GetWithCache(
      basename + '.pkl', make_frame, expires_after=datetime.timedelta(hours=3))
