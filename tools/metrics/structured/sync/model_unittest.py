#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for model.py."""

# TODO(crbug.com/40156926): Set up these tests to run on the tryjobs.

import sync.model as model
import unittest

class ModelTest(unittest.TestCase):
  """Tests for model.py."""

  def assert_project(self, project, name, id_, summary, owners,
                     key_rotation_period):
    self.assertEqual(project.name, name)
    self.assertEqual(project.id, id_)
    self.assertEqual(project.summary.strip(), summary)
    self.assertEqual(len(project.owners), len(owners))
    for actual, expected in zip(project.owners, owners):
      self.assertEqual(actual, expected)
    self.assertEqual(int(project.key_rotation_period), key_rotation_period)

  def assert_event(self, event, name, summary):
    self.assertEqual(event.name, name)
    self.assertEqual(event.summary.strip(), summary)

  def assert_metric(self, metric, name, type_, summary):
    self.assertEqual(metric.name, name)
    self.assertEqual(metric.type, type_)
    self.assertEqual(metric.summary.strip(), summary)

  def assert_model_raises(self, xml):
    raised = False
    try:
      model.Model(xml, 'chrome')
    except ValueError:
      raised = True
    self.assertTrue(raised)

  def test_valid_xml(self):
    xml = """\
          <structured-metrics>
          <project name="ProjectOne">
            <owner>test1@chromium.org</owner>
            <owner>test2@chromium.org</owner>
            <id>none</id>
            <scope>profile</scope>
            <summary> Test project. </summary>
            <key-rotation>65</key-rotation>

            <event name="EventOne">
              <summary> Test event. </summary>
              <metric name="MetricOne" type="int">
                <summary> Test metric. </summary>
              </metric>
              <metric name="MetricTwo" type="hmac-string">
                <summary> Test metric. </summary>
              </metric>
            </event>

            <event name="EventTwo">
              <summary> Test event. </summary>
              <metric name="MetricThree" type="int">
                <summary> Test metric. </summary>
              </metric>
            </event>
          </project>

          <project name="ProjectTwo">
            <owner>test@chromium.org</owner>
            <id>uma</id>
            <scope>device</scope>
            <summary> Test project. </summary>

            <event name="EventThree">
              <summary> Test event. </summary>
              <metric name="MetricFour" type="int">
                <summary> Test metric. </summary>
              </metric>
            </event>
          </project>
          </structured-metrics>"""

    data = model.Model(xml, 'chrome')

    self.assertEqual(len(data.projects), 2)
    project_one, project_two = data.projects
    self.assert_project(project_one, 'ProjectOne', 'none', 'Test project.',
                        ('test1@chromium.org', 'test2@chromium.org'), 65)
    self.assert_project(project_two, 'ProjectTwo', 'uma', 'Test project.',
                        ('test@chromium.org', ),
                        model.DEFAULT_KEY_ROTATION_PERIOD)

    self.assertEqual(len(project_one.events), 2)
    self.assertEqual(len(project_two.events), 1)
    event_one, event_two = project_one.events
    event_three, = project_two.events
    self.assert_event(event_one, 'EventOne', 'Test event.')
    self.assert_event(event_two, 'EventTwo', 'Test event.')
    self.assert_event(event_three, 'EventThree', 'Test event.')

    self.assertEqual(len(event_one.metrics), 2)
    self.assertEqual(len(event_two.metrics), 1)
    self.assertEqual(len(event_three.metrics), 1)
    metric_one, metric_two = event_one.metrics
    metric_three, = event_two.metrics
    metric_four, = event_three.metrics
    self.assert_metric(metric_one, 'MetricOne', 'int', 'Test metric.')
    self.assert_metric(metric_two, 'MetricTwo', 'hmac-string', 'Test metric.')
    self.assert_metric(metric_three, 'MetricThree', 'int', 'Test metric.')
    self.assert_metric(metric_four, 'MetricFour', 'int', 'Test metric.')

  def test_owners_validation(self):
    # No owner for project.
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="project">
        <id>uma</id>
        <summary> Test project. </summary>
        <event name="EventThree">
          <summary> Test event. </summary>
          <metric name="MetricFour" type="int">
            <summary> Test metric. </summary>
          </metric>
        </event>
        </project>
        </structured-metrics>""")

    # Owner is username not email.
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="project">
          <owner>test@</owner>
          <id>uma</id>
          <summary> Test project. </summary>
          <event name="EventThree">
            <summary> Test event. </summary>
            <metric name="MetricFour" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_id_validation(self):
    # Missing ID
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@chromium.org</owner>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

    # Invalid ID
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@chromium.org</owner>
          <id>invalid value</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_type_validation(self):
    # Missing type
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@chromium.org</owner>
          <id>none</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

    # Invalid type
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@chromium.org</owner>
          <id>none</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="invalid value">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_duplicate_summaries(self):
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@chromium.org</owner>
          <id>none</id>
          <summary> Test project. </summary>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_duplicate_project_names(self):
    # Two projects with name "Duplicate"
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="Duplicate">
          <owner>test@</owner>
          <id>uma</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        <project name="Duplicate">
          <owner>test@</owner>
          <id>uma</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_duplicate_event_names(self):
    # Two events with name "Duplicate"
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@</owner>
          <id>uma</id>
          <summary> Test project. </summary>
          <event name="Duplicate">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
          <event name="Duplicate">
            <summary> Test event. </summary>
            <metric name="MyMetric" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_duplicate_metric_names(self):
    # Two metrics with name "Duplicate"
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="MyProject">
          <owner>test@</owner>
          <id>uma</id>
          <summary> Test project. </summary>
          <event name="MyEvent">
            <summary> Test event. </summary>
            <metric name="Duplicate" type="int">
              <summary> Test metric. </summary>
            </metric>
            <metric name="Duplicate" type="int">
              <summary> Test metric. </summary>
            </metric>
          </event>
        </project>
        </structured-metrics>""")

  def test_key_rotation_validation(self):
    # Key rotation not a number.
    self.assert_model_raises("""\
        <structured-metrics>
        <project name="project">
        <id>uma</id>
        <summary> Test project. </summary>
        <owner>test@chromium.org</owner>
        <key-rotation>NaN123</key-rotation>
        <event name="EventThree">
          <summary> Test event. </summary>
          <metric name="MetricFour" type="int">
            <summary> Test metric. </summary>
          </metric>
        </event>
        </project>
        </structured-metrics>""")


if __name__ == '__main__':
  unittest.main()
