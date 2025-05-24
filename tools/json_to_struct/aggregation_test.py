#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from aggregation import AggregationKind
from aggregation import AggregationDetails
from aggregation import GetAggregationDetails
from aggregation import GenerateCCAggregation
from aggregation import GenerateHHAggregation
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
    self.assertEqual([("a", "a"), ("b", "b"), ("c", "c"), ("d", "d")],
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
    self.assertEqual([("a", "d"), ("b", "c"), ("c", "c"), ("d", "d"),
                      ("e", "c")], aggregation.GetSortedMapElements())

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

  def BuildTestElements(self, source: str, target: str,
                        count: int) -> dict[str, str]:
    """Generate a map of `count` test elements.

      Elements are named `<source>_<id>` and point to `<target>_<id>`.
      """
    res = {}
    for index in range(count):
      res[f'{source}_{index+1}'] = f'{target}_{index+1}'
    return res

  def testHHNoAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.NONE,
        name='kTestAggregation',
        elements=self.BuildTestElements('item', 'item', 5),
        export_items=True,
        map_key_type=None,
    )

    self.assertIsNone(GenerateHHAggregation('TypeName', agg))

  def testHHArrayAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.ARRAY,
        name='kTestArray',
        elements=self.BuildTestElements('item', 'item', 5),
        export_items=True,
        map_key_type=None,
    )

    self.assertEqual(
        GenerateHHAggregation('TypeName', agg).strip(),
        'extern const std::array<const TypeName*, 5> kTestArray;')

  def testHHMapAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.MAP,
        name='kTestMap',
        elements=self.BuildTestElements('src', 'tgt', 5),
        export_items=True,
        map_key_type='std::string_view',
    )

    self.assertEqual(
        GenerateHHAggregation('ValueTypeName', agg).strip(),
        'extern const base::fixed_flat_map<std::string_view, const ValueTypeName*, 5> kTestMap;'
    )

  def testCCNoAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.NONE,
        name='kTestAggregation',
        elements=self.BuildTestElements('item', 'item', 5),
        export_items=True,
        map_key_type=None,
    )

    self.assertIsNone(GenerateCCAggregation('TypeName', agg))

  def testCCArrayAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.ARRAY,
        name='kTestArray',
        elements=self.BuildTestElements('item', 'item', 3),
        export_items=True,
        map_key_type=None,
    )

    self.assertEqual(
        GenerateCCAggregation('TypeName', agg), '''
const auto kTestArray =
    std::array<const TypeName*, 3>({{
  &item_1,
  &item_2,
  &item_3,
}});
''')

  def testCCMapAggregation(self):
    agg = AggregationDetails(
        kind=AggregationKind.MAP,
        name='kTestMap',
        elements=self.BuildTestElements('src', 'tgt', 3),
        export_items=True,
        map_key_type='std::string',
    )

    self.assertEqual(
        GenerateCCAggregation('ValueTypeName', agg), '''
const auto kTestMap =
    base::MakeFixedFlatMap<std::string, const ValueTypeName*>({
  {std::string("src_1"), &tgt_1},
  {std::string("src_2"), &tgt_2},
  {std::string("src_3"), &tgt_3},
});
''')


if __name__ == '__main__':
  unittest.main()
