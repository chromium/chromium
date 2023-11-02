#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Unit tests for generator_utils.py
"""

import os
import unittest
import generator_utils

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))
TESTS_DIR = os.path.join(SCRIPT_DIR, "test_data")


class ParserTest(unittest.TestCase):
  TSV_CONTENTS = [[
      u"unique_id_A", u"", u"sender_A", u"description_A", u"trigger_A",
      u"data_A", u"destination_A", u"cookies_allowed_A", u"cookies_store_A",
      u"settings_A", u"chrome_policy_A", u"", u"source_file_A",
      u"id_hash_code_A", u"content_hash_code_A"],
    [
      u"unique_id_B", u"", u"sender_B", u"description_B", u"trigger_B",
      u"data_B", u"destination_B", u"cookies_allowed_B", u"cookies_store_B",
      u"settings_B", u"chrome_policy_B", u"", u"source_file_B",
      u"id_hash_code_B", u"content_hash_code_B"],
    [
      u"unique_id_C", u"", u"sender_C", u"description_C", u"trigger_C",
      u"data_C", u"destination_C", u"cookies_allowed_C", u"cookies_store_C",
      u"settings_C", u"chrome_policy_C", u"", u"source_file_C",
      u"id_hash_code_C", u"content_hash_code_C"]
  ]

  ANNOTATIONS_MAPPING = {
      "unique_id_A":
      generator_utils.TrafficAnnotation(
          **{
              "unique_id": "unique_id_A",
              "description": "description_A",
              "trigger": "trigger_A",
              "data": "data_A",
              "settings": "settings_A",
              "policy": "chrome_policy_A"
          }),
      "unique_id_B":
      generator_utils.TrafficAnnotation(
          **{
              "unique_id": "unique_id_B",
              "description": "description_B",
              "trigger": "trigger_B",
              "data": "data_B",
              "settings": "settings_B",
              "policy": "chrome_policy_B"
          }),
      "unique_id_C":
      generator_utils.TrafficAnnotation(
          **{
              "unique_id": "unique_id_C",
              "description": "description_C",
              "trigger": "trigger_C",
              "data": "data_C",
              "settings": "settings_C",
              "policy": "chrome_policy_C"
          })
  }

  PLACEHOLDERS = [
    {"type": generator_utils.Placeholder.GROUP, "name": "Group A"},
    {"type": generator_utils.Placeholder.SENDER, "name": "Sender 1"},
    {
      "type": generator_utils.Placeholder.ANNOTATION,
      "traffic_annotation": ANNOTATIONS_MAPPING["unique_id_A"]},
    {"type": generator_utils.Placeholder.SENDER, "name": "Sender 2"},
    {
      "type": generator_utils.Placeholder.ANNOTATION,
      "traffic_annotation": ANNOTATIONS_MAPPING["unique_id_B"]},
    {"type": generator_utils.Placeholder.GROUP, "name": "Group C"},
    {"type": generator_utils.Placeholder.SENDER, "name": "Sender 3"},
    {
      "type": generator_utils.Placeholder.ANNOTATION,
      "traffic_annotation": ANNOTATIONS_MAPPING["unique_id_C"]}
  ]

  # Document formatted according to fake_grouping.xml
  DOC_JSON = generator_utils.extract_body(target="all",
                                          json_file_path=os.path.join(
                                              TESTS_DIR, "fake_doc.json"))

  def test_load_tsv_file(self):
    self.assertEqual(self.TSV_CONTENTS, generator_utils.load_tsv_file(
      os.path.join(SRC_DIR,
      "tools/traffic_annotation/scripts/test_data/fake_annotations.tsv"),
      False))

  def test_map_annotations(self):
    self.assertEqual(self.ANNOTATIONS_MAPPING,
                     generator_utils.map_annotations(self.TSV_CONTENTS))

  def test_xml_parser_build_placeholders(self):
    xml_parser = generator_utils.XMLParser(
        os.path.join(TESTS_DIR, "fake_grouping.xml"), self.ANNOTATIONS_MAPPING)
    self.assertEqual(self.PLACEHOLDERS, xml_parser.build_placeholders())

  def test_find_first_index(self):
    first_index = generator_utils.find_first_index(self.DOC_JSON)
    self.assertEqual(1822, first_index)

  def test_find_last_index(self):
    last_index = generator_utils.find_last_index(self.DOC_JSON)
    self.assertEqual(2066, last_index)

  def test_find_chrome_browser_version(self):
    current_version = generator_utils.find_chrome_browser_version(self.DOC_JSON)
    self.assertEqual("86.0.4187.0", current_version)

  def test_find_bold_ranges(self):
    expected_bold_ranges = [(1843, 1855), (1859, 1867), (1871, 1876),
                            (1880, 1889), (1893, 1900), (1918, 1930),
                            (1934, 1942), (1968, 1975), (1946, 1951),
                            (1955, 1964), (2001, 2013), (2017, 2025),
                            (2029, 2034), (2038, 2047), (2051, 2058)]
    bold_ranges = generator_utils.find_bold_ranges(self.DOC_JSON)
    self.assertItemsEqual(expected_bold_ranges, bold_ranges)


if __name__ == "__main__":
  unittest.main()
