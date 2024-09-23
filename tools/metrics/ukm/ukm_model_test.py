#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import textwrap
import unittest

import ukm_model


PRETTY_XML = """
<!-- Comment1 -->

<ukm-configuration>

<event name="Event1">
  <owner>owner@chromium.org</owner>
  <owner>anotherowner@chromium.org</owner>
  <summary>
    Event1 summary.
  </summary>
  <metric name="Metric1">
    <owner>owner2@chromium.org</owner>
    <summary>
      Metric1 summary.
    </summary>
    <aggregation>
      <history>
        <index fields="profile.country"/>
        <index fields="profile.form_factor"/>
        <statistics>
          <quantiles type="std-percentiles"/>
        </statistics>
      </history>
    </aggregation>
  </metric>
  <metric name="Metric2">
    <aggregation>
      <history>
        <statistics export="False">
          <enumeration/>
        </statistics>
      </history>
    </aggregation>
  </metric>
  <metric name="Metric3"/>
</event>

</ukm-configuration>
""".strip()

UNPRETTIFIED_XML = """
<!-- Comment1 -->
<ukm-configuration>
<event name="Event1">
<metric name="Metric3"/>
<metric name="Metric1">
  <owner>owner2@chromium.org</owner>
  <summary>
    Metric1 summary.
  </summary>
  <aggregation>
    <history>
      <index fields="profile.form_factor"/>
      <statistics>
        <quantiles type="std-percentiles"/>
      </statistics>
      <index fields="profile.country"/>

    </history>
  </aggregation>
</metric>
        <metric name="Metric2">
          <aggregation>
            <history>
              <statistics export="False"><enumeration/></statistics>
            </history>
          </aggregation>
        </metric>

  <summary>Event1 summary.</summary>
  <owner>owner@chromium.org</owner>
  <owner>anotherowner@chromium.org</owner>

</event>
</ukm-configuration>
""".strip()

CONFIG_EVENT_NAMES_SORTED = """
<ukm-configuration>

<event name="Event1"/>

<event name="Event2"/>

<event name="Event3"/>

</ukm-configuration>
""".strip()

CONFIG_EVENT_NAMES_UNSORTED = """
<ukm-configuration>

<event name="Event2"/>

<event name="Event3"/>

<event name="Event1"/>

</ukm-configuration>
""".strip()

OBSOLETE_EVENTS_PARSED = {
    'event': {
        "obsolete": "Some message",
        "metric": {
            "summary": "Some summary",
        },
        "metric": {
            "summary": "Some summary",
        },
    },
    'event': {
        "obsolete": "Some message",
        "metric": {
            "summary": "Some summary",
        },
    },
}


class UkmXmlTest(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(UkmXmlTest, self).__init__(*args, **kwargs)
    self.maxDiff = None

  def testPrettify(self):
    result = ukm_model.PrettifyXmlAndTrimObsolete(PRETTY_XML)
    self.assertMultiLineEqual(PRETTY_XML, result.strip())
    result = ukm_model.PrettifyXmlAndTrimObsolete(UNPRETTIFIED_XML)
    self.assertMultiLineEqual(PRETTY_XML, result.strip())

  def testHasBadEventName(self):
    # Name containing illegal character.
    bad_xml = PRETTY_XML.replace('Event1', 'Event:1')
    with self.assertRaises(ValueError) as context:
      ukm_model.PrettifyXmlAndTrimObsolete(bad_xml)
    self.assertIn('Event:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

    # Name starting with a digit.
    bad_event_name_xml = PRETTY_XML.replace('Event1', '1Event')
    with self.assertRaises(ValueError) as context:
      ukm_model.PrettifyXmlAndTrimObsolete(bad_event_name_xml)
    self.assertIn('1Event', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testHasBadMetricName(self):
    # Name containing illegal character.
    bad_xml = PRETTY_XML.replace('Metric1', 'Metric:1')
    with self.assertRaises(ValueError) as context:
      ukm_model.PrettifyXmlAndTrimObsolete(bad_xml)
    self.assertIn('Metric:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

    # Name starting with a digit.
    bad_metric_name_xml = PRETTY_XML.replace('Metric3', '3rdPartyCookie')
    with self.assertRaises(ValueError) as context:
      ukm_model.PrettifyXmlAndTrimObsolete(bad_metric_name_xml)
    self.assertIn('3rdPartyCookie', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testSortByEventName(self):
    result = ukm_model.PrettifyXmlAndTrimObsolete(CONFIG_EVENT_NAMES_SORTED)
    self.assertMultiLineEqual(CONFIG_EVENT_NAMES_SORTED, result.strip())
    result = ukm_model.PrettifyXmlAndTrimObsolete(CONFIG_EVENT_NAMES_UNSORTED)
    self.assertMultiLineEqual(CONFIG_EVENT_NAMES_SORTED, result.strip())

  def testIsNotObsolete(self):
    for event in OBSOLETE_EVENTS_PARSED.values():
      self.assertFalse(ukm_model.IsNotObsolete(event))
      for metric in event.values():
        self.assertTrue(ukm_model.IsNotObsolete(metric))

  def testTrimObsoleteEvent(self):
    xml_with_obsolete_event = PRETTY_XML.replace(
        '<event name="Event1">',
        '<event name="Event1"><obsolete>Some obsoletion message.</obsolete>')
    result = ukm_model.PrettifyXmlAndTrimObsolete(xml_with_obsolete_event)

    # The event marked obsolete is trimmed from the prettified XML.
    expected = textwrap.dedent("""\
    <!-- Comment1 -->

    <ukm-configuration/>""")

    self.assertMultiLineEqual(expected, result.strip())

  def testTrimObsoleteMetric(self):
    xml_with_obsolete_metrics = PRETTY_XML.replace(
        '<metric name="Metric1">',
        '<metric name="Metric1"><obsolete>Some obsoletion message.</obsolete>')
    xml_with_obsolete_metrics = xml_with_obsolete_metrics.replace(
        '<metric name="Metric2">',
        '<metric name="Metric2"><obsolete>Some obsoletion message.</obsolete>')
    result = ukm_model.PrettifyXmlAndTrimObsolete(xml_with_obsolete_metrics)

    # The metrics marked obsolete are trimmed from the prettified XML.
    expected = textwrap.dedent("""\
    <!-- Comment1 -->

    <ukm-configuration>

    <event name="Event1">
      <owner>owner@chromium.org</owner>
      <owner>anotherowner@chromium.org</owner>
      <summary>
        Event1 summary.
      </summary>
      <metric name="Metric3"/>
    </event>

    </ukm-configuration>""")

    self.assertMultiLineEqual(expected, result.strip())


if __name__ == '__main__':
  unittest.main()
