#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for codegen.py."""

# TODO(crbug.com/40156926): Set up these tests to run on the tryjobs.

import unittest

import setup_modules  # pylint: disable=unused-import


from chromium_src.tools.metrics.structured import code_generator_cpp
from chromium_src.tools.metrics.structured.codegen_util import FileInfo, Util
from chromium_src.tools.metrics.structured.sync import model


class CodegenTest(unittest.TestCase):
  """Tests for codegen.py"""

  def test_sanitize_name(self):
    self.assertEqual(
      Util.sanitize_name('My Metric.Or::Event/Or/Project_name'),
      'My_Metric_Or__Event_Or_Project_name',
      'chrome',
    )

  def test_camel_to_snake(self):
    def check(camel, expected_snake):
      self.assertEqual(Util.camel_to_snake(camel), expected_snake)

    check('already_snake_case', 'already_snake_case')
    check('ConvertFromCamelCase', 'convert_from_camel_case')
    check('HTTPAcronymAtStart', 'http_acronym_at_start')
    check('AcronymInHTTPMiddle', 'acronym_in_http_middle')
    check('AcronymAtEndHTTP', 'acronym_at_end_http')

  def test_hash_name(self):
    # This was generated using the function in Chromium's
    # //base/metrics/metrics_hashes.cc.
    known_good_hash = 11096769389970233700
    self.assertEqual(Util.hash_name('known good hash'), known_good_hash)

  def test_event_name_hash(self):
    # This was generated using the function in Chromium's
    # //base/metrics/metrics_hashes.cc for the string
    # chrome::TestProjectOne::TestEventOne
    event_name_hash = 13593049295042080097
    project_name = 'TestProjectOne'
    event_name = 'TestEventOne'
    self.assertEqual(
      Util.event_name_hash(project_name, event_name, 'chrome'), event_name_hash
    )

  def test_validator_constexpr_tables_sorted_and_cros_handled(self):
    chrome_xml = """
      <structured-metrics>
      <project name="ZetaProject">
        <owner>test@chromium.org</owner>
        <id>per-project</id>
        <scope>profile</scope>
        <summary>Zeta project.</summary>
        <event name="BetaEvent">
          <summary>Beta event.</summary>
          <metric name="ZMetric" type="int"><summary>Z</summary></metric>
          <metric name="AMetric" type="hmac-string">
            <summary>A</summary>
          </metric>
        </event>
        <event name="AlphaEvent" force_record="true">
          <summary>Alpha event.</summary>
          <metric name="Metric" type="double"><summary>M</summary></metric>
        </event>
      </project>
      <project name="AlphaProject">
        <owner>test@chromium.org</owner>
        <id>none</id>
        <scope>device</scope>
        <summary>Alpha project.</summary>
        <event name="OnlyEvent">
          <summary>Only event.</summary>
          <metric name="RawMetric" type="raw-string">
            <summary>R</summary>
          </metric>
        </event>
      </project>
      </structured-metrics>
    """
    cros_xml = """
      <structured-metrics>
      <project name="CrosProject">
        <owner>test@chromium.org</owner>
        <id>none</id>
        <scope>device</scope>
        <summary>Cros project.</summary>
        <event name="CrosEvent">
          <summary>Cros event.</summary>
          <metric name="ArrMetric" type="int-array" max="5">
            <summary>Arr</summary>
          </metric>
        </event>
      </project>
      </structured-metrics>
    """
    merged = model.merge_models(
      model.Model(chrome_xml, 'chrome'),
      model.Model(cros_xml, 'cros'),
    )
    tmpl = code_generator_cpp.ValidatorImplTemplate(
      merged, 'out', 'structured_metrics_validator.cc'
    )
    rendered = tmpl._stamp_file(
      FileInfo('out', 'structured_metrics_validator.cc')
    )
    # Projects sorted alphabetically: AlphaProject < CrosProject < ZetaProject.
    alpha_idx = rendered.index(str(Util.hash_name('AlphaProject')))
    cros_idx = rendered.index(str(Util.hash_name('CrosProject')))
    zeta_idx = rendered.index(str(Util.hash_name('ZetaProject')))
    self.assertLess(alpha_idx, cros_idx)
    self.assertLess(cros_idx, zeta_idx)
    # Events in ZetaProject sorted: AlphaEvent < BetaEvent.
    alpha_ev = rendered.index(
      str(Util.event_name_hash('ZetaProject', 'AlphaEvent', 'chrome'))
    )
    beta_ev = rendered.index(
      str(Util.event_name_hash('ZetaProject', 'BetaEvent', 'chrome'))
    )
    self.assertLess(alpha_ev, beta_ev)
    # Metrics in BetaEvent sorted: AMetric < ZMetric.
    a_met = rendered.index(str(Util.hash_name('AMetric')))
    z_met = rendered.index(str(Util.hash_name('ZMetric')))
    self.assertLess(a_met, z_met)
    # CrosEvent disables metric validation (last bool arg = false).
    cros_ev_hash = Util.event_name_hash('CrosProject', 'CrosEvent', 'cros')
    self.assertIn(f'EventValidator(UINT64_C({cros_ev_hash}),', rendered)
    self.assertIn(', false)', rendered)

  def test_is_chromeos_only_project(self):
    xml = """
      <structured-metrics>
      <project name="CrOSEvents" cros_events="true">
        <owner>test@chromium.org</owner>
        <id>none</id>
        <scope>device</scope>
        <summary>Sequenced project.</summary>
        <event name="SeqEvent"><summary>S</summary></event>
      </project>
      <project name="PhoneHub">
        <owner>test@chromium.org</owner>
        <id>none</id>
        <scope>device</scope>
        <summary>ChromeOS project.</summary>
        <event name="Ev">
          <summary>E</summary>
          <metric name="M" type="int"><summary>M</summary></metric>
        </event>
      </project>
      <project name="Glic">
        <owner>test@chromium.org</owner>
        <id>none</id>
        <scope>profile</scope>
        <summary>Cross-platform project.</summary>
        <event name="Ev">
          <summary>E</summary>
          <metric name="M" type="int"><summary>M</summary></metric>
        </event>
      </project>
      </structured-metrics>
    """
    parsed = model.Model(xml, 'chrome')
    kept = [
      p.name for p in parsed.projects if not Util.is_chromeos_only_project(p)
    ]
    self.assertEqual(kept, ['Glic'])


if __name__ == '__main__':
  unittest.main()
