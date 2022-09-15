# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import xml_validations
from xml.dom import minidom


class UkmXmlValidationTest(unittest.TestCase):

  def toUkmConfig(self, xml_string):
    dom = minidom.parseString(xml_string)
    [ukm_config] = dom.getElementsByTagName('ukm-configuration')
    return ukm_config

  def testEventsHaveOwners(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1">
            <owner>dev@chromium.org</owner>
          </event>
        </ukm-configuration>
        """.strip())
    validator = xml_validations.UkmXmlValidation(ukm_config)
    success, errors = validator.checkEventsHaveOwners()
    self.assertTrue(success)
    self.assertListEqual([], errors)

  def testEventsMissingOwners(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1"/>
          <event name="Event2">
            <owner></owner>
          </event>
          <event name="Event3">
            <owner>johndoe</owner>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
        "<owner> tag is required for event 'Event1'.",
        "<owner> tag for event 'Event2' should not be empty.",
        "<owner> tag for event 'Event3' expects a Chromium or Google email "
        "address.",
    ]

    validator = xml_validations.UkmXmlValidation(ukm_config)
    success, errors = validator.checkEventsHaveOwners()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

  def testMetricHasUndefinedEnum(self):
    ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event1">
            <metric name="Metric2" enum="FeatureObserver"/>
          </event>
          <event name="Event2">
            <metric name="Metric1" enum="BadEnum"/>
            <metric name="Metric2" enum="FeatureObserver"/>
            <metric name="Metric3" unit="ms"/>
            <metric name="Metric4"/>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
        "Unknown enum BadEnum in ukm metric Event2:Metric1.",
    ]

    expected_warnings = [
        "Warning: Neither 'enum' or 'unit' is specified for ukm metric "
        "Event2:Metric4.",
    ]

    validator = xml_validations.UkmXmlValidation(ukm_config)
    success, errors, warnings = validator.checkMetricTypeIsSpecified()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)
    self.assertListEqual(expected_warnings, warnings)

  def testCheckLocalMetricIsAggregated(self):
    bad_ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event">
            <metric name="M1" enum="Enum1"/>
            <metric name="M2" enum="Enum2">
              <aggregation>
                <history>
                  <index fields="metrics.M1,metrics.M4,profile.country"/>
                  <statistics>
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
            <metric name="M3" unit="ms">
              <aggregation>
                <history>
                  <index fields="metrics.M1,metrics.M2,metrics.M4"/>
                  <statistics>
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
            <metric name="M4"/>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
      xml_validations.INVALID_LOCAL_METRIC_FIELD_ERROR %(
        {'event':'Event', 'metric':'M2', 'invalid_metrics':'M1, M4'}),
      xml_validations.INVALID_LOCAL_METRIC_FIELD_ERROR %(
        # M3 is not included in invalid_metrics because it's configured to
        # aggregate as an enumeration.
        {'event':'Event', 'metric':'M3', 'invalid_metrics':'M1, M4'}),
    ]
    validator = xml_validations.UkmXmlValidation(bad_ukm_config)
    success, errors = validator.checkLocalMetricIsAggregated()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

    # Add aggregation definitions to M1 and M4 to make it valid. Note the
    # export="False" that prevents M1 and M4 from being aggregated, and only
    # useful as an index field.
    good_ukm_config = self.toUkmConfig("""
        <ukm-configuration>
          <event name="Event">
            <metric name="M1" enum="Enum1">
              <aggregation>
                <history>
                  <statistics export="False">
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
            <metric name="M2" enum="Enum2">
              <aggregation>
                <history>
                  <index fields="metrics.M1,metrics.M4,profile.country"/>
                  <statistics>
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
            <metric name="M3" unit="ms">
              <aggregation>
                <history>
                  <index fields="metrics.M1,metrics.M2,metrics.M4"/>
                  <statistics>
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
            <metric name="M4">
              <aggregation>
                <history>
                  <statistics export="False">
                    <enumeration/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
          </event>
        </ukm-configuration>
        """.strip())

    validator = xml_validations.UkmXmlValidation(good_ukm_config)
    success, errors = validator.checkLocalMetricIsAggregated()
    self.assertTrue(success)
    self.assertListEqual([], errors)


if __name__ == '__main__':
  unittest.main()
