# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class Metric(object):
  """Base class for all the metrics that are used by telemetry measurements.

  The Metric class represents a way of measuring something. Metrics are
  helper classes used by PageTests. Each PageTest may use
  multiple metrics; each metric should be focused on collecting data
  about one thing.
  """

  def Start(self, page, tab):
    """Start collecting data for this metric."""
    pass

  def Stop(self, page, tab):
    """Stop collecting data for this metric (if applicable)."""
    pass

  def AddResults(self, tab, results):
    """Add the data collected into the results object for a measurement.

    Metrics may implement AddResults to provide a common way to add results
    to the PageTestResults in PageTest.ValidateOrMeasurePage --
    results should be added with results.AddMeasurement(...).
    """
    raise NotImplementedError()
