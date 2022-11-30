#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for update_annotations_doc.py
"""

import os
import sys
import unittest
from mock import MagicMock

# Mock some imports which aren't necessary during testing.
sys.modules["infra_libs"] = MagicMock()
import update_annotations_doc
import generator_utils

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))
TESTS_DIR = os.path.join(SCRIPT_DIR, "test_data")


class UpdateAnnotationsDocTest(unittest.TestCase):
  network_doc_obj = update_annotations_doc.NetworkTrafficAnnotationsDoc(
    "", "", "", "", "")

  def test_create_group_request(self):
    text = "TestGroup"
    req, index = self.network_doc_obj._create_group_or_sender_request(
        text, 0, generator_utils.Placeholder.GROUP)

    self.assertEqual(len(text)+1, index)
    expected_req = [
      {"insertText": {"text": "TestGroup\n", "location": {"index": 0}}},
      {"updateParagraphStyle": {
        "fields": "*",
        "range": {"endIndex": 10, "startIndex": 0},
        "paragraphStyle": {
          "spacingMode": "NEVER_COLLAPSE",
          "direction": "LEFT_TO_RIGHT",
          "namedStyleType": "HEADING_1",
          "spaceAbove": {"unit": "PT"}
          }
        }
      },
      {"updateTextStyle": {
        "textStyle": {
          "fontSize": {"magnitude": 20, "unit": "PT"},
          "bold": False,
          "weightedFontFamily": {
            "fontFamily": "Roboto",
            "weight": 400
          }
        },
        "range": {"endIndex": 10, "startIndex": 0}, "fields": "*"}}
    ]
    self.assertEqual(expected_req, req)

  def test_create_sender_request(self):
    text = "TestSender"
    print(text)
    req, index = self.network_doc_obj._create_group_or_sender_request(
        text, 0, generator_utils.Placeholder.SENDER)

    self.assertEqual(len(text)+1, index)
    expected_req = [
      {"insertText": {"text": "TestSender\n", "location": {"index": 0}}},
      {"updateParagraphStyle": {
        "fields": "*",
        "range": {"endIndex": 11, "startIndex": 0},
        "paragraphStyle": {
          "spacingMode": "NEVER_COLLAPSE",
          "direction": "LEFT_TO_RIGHT",
          "namedStyleType": "HEADING_2",
          "spaceAbove": {"unit": "PT"}
          }
        }
      },
      {"updateTextStyle": {
        "textStyle": {"fontSize": {"magnitude": 14, "unit": "PT"},
        "bold": True,
        "weightedFontFamily": {"fontFamily": "Roboto", "weight": 400}},
        "range": {"endIndex": 11, "startIndex": 0}, "fields": "*"}
        }
    ]
    self.assertEqual(expected_req, req)

  def test_create_annotation_request(self):
    traffic_annotation = generator_utils.TrafficAnnotation(
        **{
            "unique_id": "unique_id_A",
            "description": "description_A",
            "trigger": "trigger_A",
            "data": "data_A",
            "settings": "settings_A",
            "policy": "chrome_policy_A"
        })

    req, index = self.network_doc_obj._create_annotation_request(
      traffic_annotation, 0)

    self.assertEqual(109, index)
    expected_req = [
      {'insertText': {'text': '\n', 'location': {'index': 0}}},
      {'insertTable': {'rows': 1, 'location': {'index': 0}, 'columns': 2}},
      {'insertText': {'text': 'unique_id_A', 'location': {'index': 4}}},
      {
        'insertText': {
          'text': "description_A\nTrigger: trigger_A\nData: data_A\nSettings: "
          "settings_A\nPolicy: chrome_policy_A", 'location': {'index': 17}}},
      {'updateTableColumnProperties': {
        'columnIndices': [0],
        'fields': '*',
        'tableColumnProperties': {
          'width': {'magnitude': 153, 'unit': 'PT'},
          'widthType': 'FIXED_WIDTH'},
          'tableStartLocation': {'index': 1}}},
      {'updateTableColumnProperties': {
        'columnIndices': [1],
        'fields': '*',
        'tableColumnProperties': {
          'width': {'magnitude': 534, 'unit': 'PT'},'widthType': 'FIXED_WIDTH'},
        'tableStartLocation': {'index': 1}}},
      {'updateTableCellStyle': {
        'fields': '*',
        'tableCellStyle': {
          'rowSpan': 1,
          'borderBottom': {
            'color': {
              'color': {'rgbColor': {'blue': 1.0, 'green': 1.0, 'red': 1.0}}},
            'width': {'unit': 'PT'}, 'dashStyle': 'SOLID'},
          'paddingBottom': {'magnitude': 1.44, 'unit': 'PT'},
          'paddingLeft': {'magnitude': 1.44, 'unit': 'PT'},
          'paddingTop': {'magnitude': 1.44, 'unit': 'PT'},
          'borderLeft': {
            'color': {
              'color': {'rgbColor': {'blue': 1.0, 'green': 1.0, 'red': 1.0}}},
            'width': {'unit': 'PT'},
            'dashStyle': 'SOLID'},
          'columnSpan': 1,
          'backgroundColor': {
            'color': {'rgbColor': {'blue': 1.0, 'green': 1.0, 'red': 1.0}}},
          'borderRight': {
            'color': {
              'color': {'rgbColor': {'blue': 1.0, 'green': 1.0, 'red': 1.0}}},
            'width': {'unit': 'PT'},
            'dashStyle': 'SOLID'},
          'borderTop': {
            'color': {
              'color': {'rgbColor': {'blue': 1.0, 'green': 1.0, 'red': 1.0}}},
              'width': {'unit': 'PT'},
              'dashStyle': 'SOLID'},
          'paddingRight': {'magnitude': 1.44, 'unit': 'PT'}},
          'tableStartLocation': {'index': 1}}},
        {'updateParagraphStyle': {
          'fields': '*',
          'range': {'endIndex': 108, 'startIndex': 4},
          'paragraphStyle': {
            'spacingMode': 'NEVER_COLLAPSE',
            'direction': 'LEFT_TO_RIGHT',
            'spaceBelow': {'magnitude': 4, 'unit': 'PT'},
            'lineSpacing': 100,
            'avoidWidowAndOrphan': False,
            'namedStyleType': 'NORMAL_TEXT'}}},
        {'updateTextStyle': {
          'textStyle': {'fontSize': {'magnitude': 9, 'unit': 'PT'},
          'bold': False,
          'weightedFontFamily': {'fontFamily': 'Roboto', 'weight': 400}},
          'range': {'endIndex': 108, 'startIndex': 4}, 'fields': '*'}}]
    self.assertEqual(expected_req, req)


if __name__ == "__main__":
  unittest.main()
