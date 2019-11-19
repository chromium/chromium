# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module


class PressStory(page_module.Page):
  """Base class for Press stories.

    Override ExecuteTest to execute javascript on the page and
    ParseTestResults to obtain javascript metrics from page.

    Example Implementation:

    class FooPressStory:
      URL = 'http://example.com/foo_story'
      NAME = 'FooStory'

      def ExecuteTest(self, action_runner):
        // Execute some javascript

      def ParseTestResults(self, action_runner):
        js_code = 'some_js_expression;'
        self.AddJavaScriptMeasurement(name, unit, js_code)

  """
  URL = None
  DETERMINISTIC_JS = False
  NAME = None

  def __init__(self, ps):
    super(PressStory, self).__init__(
        self.URL, ps,
        base_dir=ps.base_dir,
        make_javascript_deterministic=self.DETERMINISTIC_JS,
        name=self.NAME if self.NAME else self.URL)
    self._measurements = []
    self._action_runner = None

  def AddMeasurement(self, name, unit, samples, description=None):
    """Record an ad-hoc measurement.

    Args:
      name: A string with the name of the measurement (e.g. 'score', 'runtime',
        etc).
      unit: A string specifying the unit used for measurements (e.g. 'ms',
        'count', etc).
      samples: Either a single numeric value or a list of numeric values to
        record as part of this measurement.
      description: An optional string with a short human readable description
        of the measurement.
    """
    # TODO(crbug.com/999484): Ideally, these should be recorded directly into
    # the results object, rather than held on this temporary list. That needs,
    # however, another slight refactor to make the results object available at
    # this point.
    self._measurements.append({'name': name, 'unit': unit, 'samples': samples,
                               'description': description})

  def AddJavaScriptMeasurement(self, name, unit, code, **kwargs):
    """Run some JavaScript to obtain and record an ad-hoc measurements.

    Args:
      name: A string with the name of the measurement (e.g. 'score', 'runtime',
        etc).
      unit: A string specifying the unit used for measurements (e.g. 'ms',
        'count', etc).
      code: A piece of JavaScript code to run on the current tab, it must
        return either a single or a list of numeric values. These are the
        values for the measurement to be recorded.
      description: An optional string with a short human readable description
        of the measurement.
      Other keyword arguments provide values to be interpolated within
          the JavaScript code. See telemetry.util.js_template for details.
    """
    description = kwargs.pop('description', None)
    samples = self._action_runner.EvaluateJavaScript(code, **kwargs)
    self.AddMeasurement(name, unit, samples, description)

  def GetMeasurements(self):
    return self._measurements

  def ExecuteTest(self, action_runner):
    pass

  def ParseTestResults(self, action_runner):
    pass

  def RunPageInteractions(self, action_runner):
    self._action_runner = action_runner
    try:
      self.ExecuteTest(action_runner)
      self.ParseTestResults(action_runner)
    finally:
      self._action_runner = None
