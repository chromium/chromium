# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from xml.dom import minidom

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.ukm.xml_validations as xml_validations

class UkmXmlValidationTest(unittest.TestCase):

  def to_ukm_config(self, xml_string):
    dom = minidom.parseString(xml_string)
    [ukm_config] = dom.getElementsByTagName('ukm-configuration')
    return ukm_config

  def test_events_have_owners(self):
    ukm_config = self.to_ukm_config("""
        <ukm-configuration>
          <event name="Event1">
            <owner>dev@chromium.org</owner>
          </event>
        </ukm-configuration>
        """.strip())
    validator = xml_validations.UkmXmlValidation(ukm_config)
    success, errors = validator.check_events_have_owners()
    self.assertTrue(success)
    self.assertListEqual([], errors)

  def test_events_missing_owners(self):
    ukm_config = self.to_ukm_config("""
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
    success, errors = validator.check_events_have_owners()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

  def test_metric_has_undefined_enum(self):
    ukm_config = self.to_ukm_config("""
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
    success, errors, warnings = validator.check_metric_type_is_specified()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)
    self.assertListEqual(expected_warnings, warnings)

  def test_check_local_metric_is_aggregated(self):
    bad_ukm_config = self.to_ukm_config("""
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
    success, errors = validator.check_local_metric_is_aggregated()
    self.assertFalse(success)
    self.assertListEqual(expected_errors, errors)

    # Add aggregation definitions to M1 and M4 to make it valid. Note the
    # export="False" that prevents M1 and M4 from being aggregated, and only
    # useful as an index field.
    good_ukm_config = self.to_ukm_config("""
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
    success, errors = validator.check_local_metric_is_aggregated()
    self.assertTrue(success)
    self.assertListEqual([], errors)


  parameters_test_statistics = [
      # Valid configuration with <enumeration/>
      (
          # config
          """
      <ukm-configuration>
      <event name="Test.Event">
        <metric name="Test.Metric">
          <aggregation>
            <history>
              <statistics>
                <enumeration/>
              </statistics>
            </history>
          </aggregation>
        </metric>
      </event>
      </ukm-configuration>""".strip(),
          # expected_success
          True,
          # expected_errors
          []),
      # Invalid configuration with an empty <statistics/> tag.
      (
          # config
          """
      <ukm-configuration>
        <event name="Test.Event">
          <metric name="Test.Metric">
            <aggregation>
              <history>
                <statistics/>
              </history>
            </aggregation>
          </metric>
        </event>
      </ukm-configuration>""".strip(),
          # expected_success
          False,
          # expected_errors
          [
              'Invalid statistics field specification in ukm.xml, in metric '
              'Test.Event:Test.Metric. To have a metric aggregated, '
              'aggregation, history and statistics tags need to be added along'
              ' with the type of statistic. See https://chromium.googlesource.'
              'com/chromium/src.git/+/main/services/metrics/ukm_api.md#'
              'controlling-the-aggregation-of-metrics.'
          ]),
      # Invalid configuration with <wrong_tag/>.
      (
          # config
          """
        <ukm-configuration>
          <event name="Test.Event">
            <metric name="Test.Metric">
              <aggregation>
                <history>
                  <statistics>
                    <wrong_tag/>
                  </statistics>
                </history>
              </aggregation>
            </metric>
          </event>
        </ukm-configuration>""".strip(),
          # expected_success
          False,
          # expected_errors
          [
              'Invalid statistics field specification in ukm.xml, in metric '
              'Test.Event:Test.Metric. To have a metric aggregated, '
              'aggregation, history and statistics tags need to be added along'
              ' with the type of statistic. See https://chromium.googlesource.'
              'com/chromium/src.git/+/main/services/metrics/ukm_api.md#'
              'controlling-the-aggregation-of-metrics.'
          ]),
      # Invalid configuration with <quantiles/> and wrong type.
      (
          # config
          """
      <ukm-configuration>
        <event name="Test.Event">
          <metric name="Test.Metric">
            <aggregation>
              <history>
                <statistics>
                  <quantiles type="wrong-type"/>
                </statistics>
              </history>
            </aggregation>
          </metric>
        </event>
      </ukm-configuration>""".strip(),
          # expected_success
          False,
          # expected_errors
          [
              'Invalid statistics field specification in ukm.xml, in metric '
              'Test.Event:Test.Metric. To have a metric aggregated, '
              'aggregation, history and statistics tags need to be added along'
              ' with the type of statistic. See https://chromium.googlesource.'
              'com/chromium/src.git/+/main/services/metrics/ukm_api.md#'
              'controlling-the-aggregation-of-metrics.'
          ])
  ]

  def test_statistics_non_empty_valid(self):
    """Validates passed parameters against check_statistics_non_empty_valid."""
    for config, ex_success, ex_error in self.parameters_test_statistics:
      ukm_config = self.to_ukm_config(config)
      validator = xml_validations.UkmXmlValidation(ukm_config)

      result_success, result_error = (
          validator.check_statistics_non_empty_valid())
      self.assertTrue(result_success) if ex_success else self.assertFalse(
          result_success)
      self.assertListEqual(ex_error, result_error)

  def test_metric_uses_forbidden_name(self):
    """Validates that metrics using forbidden names generate errors."""
    bad_ukm_config = self.to_ukm_config("""
        <ukm-configuration>
          <event name="Event1">
            <metric name="Event" enum="SomeEnumName"/>
            <metric name="event" enum="SomeEnumName"/>
            <metric name="EVENT" enum="SomeEnumName"/>
            <metric name="Metadata" enum="SomeEnumName"/>
            <metric name="metadata" enum="SomeEnumName"/>
            <metric name="SomeGoodName" enum="SomeEnumName"/>
          </event>
        </ukm-configuration>
        """.strip())
    expected_errors = [
        "Metric name 'Event' in event 'Event1' collides with a "
        "UKM-internal keyword. Please pick a different name.",
        "Metric name 'event' in event 'Event1' collides with a "
        "UKM-internal keyword. Please pick a different name.",
        "Metric name 'EVENT' in event 'Event1' collides with a "
        "UKM-internal keyword. Please pick a different name.",
        "Metric name 'Metadata' in event 'Event1' collides with a "
        "UKM-internal keyword. Please pick a different name.",
        "Metric name 'metadata' in event 'Event1' collides with a "
        "UKM-internal keyword. Please pick a different name.",
    ]
    validator = xml_validations.UkmXmlValidation(bad_ukm_config)
    is_success, errors = validator.check_metric_names()
    self.assertFalse(is_success)
    self.assertListEqual(expected_errors, errors)


if __name__ == '__main__':
  unittest.main()
