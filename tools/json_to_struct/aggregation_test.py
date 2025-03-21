#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from aggregation import AggregationKind, AggregationDetails, GetAggregationDetails
import unittest


class AggregationTest(unittest.TestCase):

  def testDefaults(self):
    self.assertEqual(GetAggregationDetails({}),
                     AggregationDetails(AggregationKind.NONE, None, True))

  def testGenerateOldStyleArray(self):
    self.assertEqual(
        GetAggregationDetails({"generate_array": {
            "array_name": "TestArray"
        }}), AggregationDetails(AggregationKind.ARRAY, "TestArray", True))

  def testGenerateArray(self):
    self.assertEqual(
        GetAggregationDetails(
            {"aggregation": {
                "type": "array",
                "name": "TestArray"
            }}), AggregationDetails(AggregationKind.ARRAY, "TestArray", True))

  def testGenerateArrayAndHideElements(self):
    self.assertEqual(
        GetAggregationDetails({
            "aggregation": {
                "type": "array",
                "name": "TestArray",
                "export_items": False
            }
        }), AggregationDetails(AggregationKind.ARRAY, "TestArray", False))

  def testGenerateMap(self):
    self.assertEqual(
        GetAggregationDetails(
            {"aggregation": {
                "type": "map",
                "name": "TestMap"
            }}), AggregationDetails(AggregationKind.MAP, "TestMap", True))

  def testGenerateMapAndHideElements(self):
    self.assertEqual(
        GetAggregationDetails({
            "aggregation": {
                "type": "map",
                "name": "TestMap",
                "export_items": False
            }
        }), AggregationDetails(AggregationKind.MAP, "TestMap", False))

  def testUnrecognizedAggregationType(self):
    self.assertRaisesRegex(
        Exception, "'collection' is not a valid AggregationKind",
        lambda: GetAggregationDetails({"aggregation": {
            "type": "collection",
        }}))

  def testArrayMustHaveAName(self):
    self.assertRaisesRegex(
        Exception, "Aggregation container needs a `name`.",
        lambda: GetAggregationDetails({"aggregation": {
            "type": "array",
        }}))

  def testMapMustHaveAName(self):
    self.assertRaisesRegex(
        Exception, "Aggregation container needs a `name`.",
        lambda: GetAggregationDetails({"aggregation": {
            "type": "map",
        }}))


if __name__ == '__main__':
  unittest.main()
