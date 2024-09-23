#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import dwa_model

PRETTY_XML = """
<!-- Comment1 -->

<dwa-configuration>

<event name="Event1">
  <owner>owner@chromium.org</owner>
  <owner>anotherowner@chromium.org</owner>
  <summary>
    Event1 summary.
  </summary>
  <study name="Study1"/>
  <study name="Study2"/>
  <metric name="Metric1">
    <summary>
      Metric1 summary.
    </summary>
  </metric>
  <metric name="Metric2"/>
  <metric name="Metric3"/>
</event>

</dwa-configuration>
""".strip()

UNPRETTIFIED_XML = """
<!-- Comment1 -->
<dwa-configuration>
<event name="Event1">
<metric name="Metric3"/>
<metric name="Metric1">
  <summary>
    Metric1 summary.
  </summary>
</metric>
        <metric name="Metric2">
        </metric>

  <summary>Event1 summary.</summary>
  <owner>owner@chromium.org</owner>
  <owner>anotherowner@chromium.org</owner>

  <study name="Study2"/>
<study name="Study1"/>

</event>
</dwa-configuration>
""".strip()

CONFIG_EVENT_NAMES_SORTED = """
<dwa-configuration>

<event name="Event1"/>

<event name="Event2"/>

<event name="Event3"/>

</dwa-configuration>
""".strip()

CONFIG_EVENT_NAMES_UNSORTED = """
<dwa-configuration>

<event name="Event2"/>

<event name="Event3"/>

<event name="Event1"/>

</dwa-configuration>
""".strip()


class DwaXmlTest(unittest.TestCase):

  def __init__(self, *args, **kwargs) -> None:
    super(DwaXmlTest, self).__init__(*args, **kwargs)
    self.maxDiff = None

  def testPrettify(self) -> None:
    result = dwa_model.PrettifyXML(PRETTY_XML)
    self.assertMultiLineEqual(PRETTY_XML, result.strip())
    result = dwa_model.PrettifyXML(UNPRETTIFIED_XML)
    self.assertMultiLineEqual(PRETTY_XML, result.strip())

  def testHasBadEventName(self) -> None:
    # Name containing illegal character.
    bad_xml = PRETTY_XML.replace('Event1', 'Event:1')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_xml)
    self.assertIn('Event:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

    # Name starting with a digit.
    bad_event_name_xml = PRETTY_XML.replace('Event1', '1Event')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_event_name_xml)
    self.assertIn('1Event', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testHasBadMetricName(self) -> None:
    # Name containing illegal character.
    bad_xml = PRETTY_XML.replace('Metric1', 'Metric:1')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_xml)
    self.assertIn('Metric:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

    # Name starting with a digit.
    bad_metric_name_xml = PRETTY_XML.replace('Metric3', '3rdPartyCookie')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_metric_name_xml)
    self.assertIn('3rdPartyCookie', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testHasBadStudyName(self) -> None:
    # Name containing illegal character.
    bad_xml = PRETTY_XML.replace('Study1', 'Study:1')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_xml)
    self.assertIn('Study:1', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

    # Name starting with a digit.
    bad_study_name_xml = PRETTY_XML.replace('Study2', '3rdPartyCookie')
    with self.assertRaises(ValueError) as context:
      dwa_model.PrettifyXML(bad_study_name_xml)
    self.assertIn('3rdPartyCookie', str(context.exception))
    self.assertIn('does not match regex', str(context.exception))

  def testSortByEventName(self) -> None:
    result = dwa_model.PrettifyXML(CONFIG_EVENT_NAMES_SORTED)
    self.assertMultiLineEqual(CONFIG_EVENT_NAMES_SORTED, result.strip())
    result = dwa_model.PrettifyXML(CONFIG_EVENT_NAMES_UNSORTED)
    self.assertMultiLineEqual(CONFIG_EVENT_NAMES_SORTED, result.strip())


if __name__ == '__main__':
  unittest.main()
