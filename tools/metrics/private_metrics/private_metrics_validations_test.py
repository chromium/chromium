#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import xml.dom.minidom
import private_metrics_validations


class EventBasedXmlValidationTest(unittest.TestCase):

  def parseConfig(self, xml_string: str) -> xml.dom.minidom.Element:
    dom = xml.dom.minidom.parseString(xml_string)
    [dwa_config] = dom.getElementsByTagName('test-configuration')
    return dwa_config

  def testEventsHaveOwners(self) -> None:
    dwa_config = self.parseConfig("""
        <test-configuration>
          <event name="Event1">
            <owner>dev@chromium.org</owner>
          </event>
        </test-configuration>
        """.strip())
    validator = private_metrics_validations.EventBasedXmlValidation(
        dwa_config, "test")
    success, errors = validator.checkEventsHaveOwners()
    self.assertTrue(success)
    self.assertListEqual([], errors)

  def testEventsMissingOwners(self) -> None:
    dwa_config = self.parseConfig("""
        <test-configuration>
          <event name="Event1"/>
          <event name="Event2">
            <owner></owner>
          </event>
          <event name="Event3">
            <owner>johndoe</owner>
          </event>
        </test-configuration>
        """.strip())
    expected_errors = [
        "<owner> tag is required for event 'Event1'.",
        "<owner> tag for event 'Event2' should not be empty.",
        "<owner> tag for event 'Event3' expects a Chromium or Google email "
        "address.",
    ]

    validator = private_metrics_validations.EventBasedXmlValidation(
        dwa_config, "test")
    success, errors = validator.checkEventsHaveOwners()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

  def testMetricHasUndefinedEnum(self) -> None:
    config_xml = self.parseConfig("""
        <test-configuration>
          <event name="Event1">
            <metric name="Metric2" enum="FeatureObserver"/>
          </event>
          <event name="Event2">
            <metric name="Metric1" enum="BadEnum"/>
            <metric name="Metric2" enum="FeatureObserver"/>
            <metric name="Metric3"/>
            <metric name="Metric4"/>
          </event>
        </test-configuration>
        """.strip())
    expected_errors = [
        "Unknown enum BadEnum in test metric Event2:Metric1.",
    ]

    validator = private_metrics_validations.EventBasedXmlValidation(
        config_xml, "test")
    success, errors = validator.checkMetricTypeIsSpecified()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)


if __name__ == '__main__':
  unittest.main()
