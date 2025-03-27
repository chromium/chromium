#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from aggregation import AggregationKind, AggregationDetails, GetAggregationDetails
import unittest


class AggregationTest(unittest.TestCase):

  def testDefaults(self):
    self.assertEqual(
        GetAggregationDetails({}),
        AggregationDetails(AggregationKind.NONE, None, True, {}, None))

  def testGenerateArray(self):
    self.assertEqual(
        GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "array",
                "name": "TestArray"
            }
        }),
        AggregationDetails(AggregationKind.ARRAY, "TestArray", True, {}, None))

  def testGenerateArrayAndHideElements(self):
    self.assertEqual(
        GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "array",
                "name": "TestArray",
                "export_items": False
            }
        }),
        AggregationDetails(AggregationKind.ARRAY, "TestArray", False, {}, None))

  def testGenerateMapWithDefaultKeyType(self):
    self.assertEqual(
        GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "map",
                "name": "TestMap"
            }
        }),
        AggregationDetails(AggregationKind.MAP, "TestMap", True, {},
                           "std::string_view"))

  def testGenerateMapWithSuppliedKeyType(self):
    self.assertEqual(
        GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "map",
                "name": "TestMap",
                "map_key_type": "chrome::CustomKey"
            }
        }),
        AggregationDetails(AggregationKind.MAP, "TestMap", True, {},
                           "chrome::CustomKey"))

  def testGenerateMapAndHideElements(self):
    self.assertEqual(
        GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "map",
                "name": "TestMap",
                "export_items": False
            }
        }),
        AggregationDetails(AggregationKind.MAP, "TestMap", False, {},
                           "std::string_view"))

  def testGetSortedArrayElements(self):
    aggregation = GetAggregationDetails({
        "elements": {
            "c": [],
            "d": [],
            "b": [],
            "a": []
        },
        "aggregation": {
            "type": "array",
            "name": "TestMap"
        }
    })

    self.assertEqual(["a", "b", "c", "d"], aggregation.GetSortedArrayElements())

  def testGetSortedMapElementsNoAliases(self):
    aggregation = GetAggregationDetails({
        "elements": {
            "c": [],
            "d": [],
            "b": [],
            "a": []
        },
        "aggregation": {
            "type": "map",
            "name": "TestMap"
        }
    })

    # Expect the result to be sorted as well.
    self.assertEqual([["a", "a"], ["b", "b"], ["c", "c"], ["d", "d"]],
                     aggregation.GetSortedMapElements())

  def testGetSortedMapElementsWithAliases(self):
    aggregation = GetAggregationDetails({
        "elements": {
            "c": [],
            "d": [],
        },
        "aggregation": {
            "type": "map",
            "name": "TestMap",
            "map_aliases": {
                "a": "d",
                "b": "c",
                "e": "c",
            }
        }
    })

    # Expect the result to be sorted as well.
    self.assertEqual(
        [["a", "d"], ["b", "c"], ["c", "c"], ["d", "d"], ["e", "c"]],
        aggregation.GetSortedMapElements())

  def testGenerateMapWithConflictingAliases(self):
    self.assertRaisesRegex(
        Exception, "Alias `b` already defined as element.",
        lambda: GetAggregationDetails({
            "elements": {
                "a": [],
                "b": [],
            },
            "aggregation": {
                "type": "map",
                "name": "TestMap",
                "map_aliases": {
                    "b": "a"
                }
            }
        }))

  def testGenerateMapWithIncorrectAliases(self):
    self.assertRaisesRegex(
        Exception, "Aliased element `b` does not exist.",
        lambda: GetAggregationDetails({
            "elements": {
                "a": [],
            },
            "aggregation": {
                "type": "map",
                "name": "TestMap",
                "map_aliases": {
                    "b": "a",
                    "c": "b"
                }
            }
        }))

  def testUnrecognizedAggregationType(self):
    self.assertRaisesRegex(
        Exception, "'collection' is not a valid AggregationKind",
        lambda: GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "collection",
            }
        }))

  def testArrayMustHaveAName(self):
    self.assertRaisesRegex(
        Exception, "Aggregation container needs a `name`.",
        lambda: GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "array",
            }
        }))

  def testMapMustHaveAName(self):
    self.assertRaisesRegex(
        Exception, "Aggregation container needs a `name`.",
        lambda: GetAggregationDetails({
            "elements": {},
            "aggregation": {
                "type": "map",
            }
        }))


if __name__ == '__main__':
  unittest.main()
