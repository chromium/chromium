#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import web_idl_schema


def getFunction(schema, name):
  for item in schema['functions']:
    if item['name'] == name:
      return item
  raise KeyError('Missing function %s' % name)


def getReturns(schema, name):
  function = getFunction(schema, name)
  return function.get('returns', None)


class WebIdlSchemaTest(unittest.TestCase):

  def setUp(self):
    loaded = web_idl_schema.Load('test/web_idl/basics.idl')
    self.assertEqual(1, len(loaded))
    self.assertEqual('testWebIdl', loaded[0]['namespace'])
    self.idl_basics = loaded[0]

  def testFunctionReturnTypes(self):
    schema = self.idl_basics
    # Test basic types.
    self.assertEqual(
        None,
        getReturns(schema, 'returnsVoid'),
    )
    self.assertEqual(
        {'name': 'returnsBoolean', 'type': 'boolean'},
        getReturns(schema, 'returnsBoolean'),
    )
    self.assertEqual(
        {'name': 'returnsDouble', 'type': 'number'},
        getReturns(schema, 'returnsDouble'),
    )
    self.assertEqual(
        {'name': 'returnsLong', 'type': 'integer'},
        getReturns(schema, 'returnsLong'),
    )
    self.assertEqual(
        {'name': 'returnsDOMString', 'type': 'string'},
        getReturns(schema, 'returnsDOMString'),
    )

  # TODO(crbug.com/340297705): This will eventually be relaxed when adding
  # support for shared types to the new parser.
  def testMissingBrowserInterface(self):
    expected_error_regex = 'Required partial Browser interface not found in .*'
    self.assertRaisesRegex(
        Exception,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_browser_interface.idl',
    )

  def testMissingAttributeOnBrowser(self):
    expected_error_regex = (
        'The Browser interface should have exactly one '
        'attribute for the name the API will be exposed under in .*'
    )
    self.assertRaisesRegex(
        Exception,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_attribute_on_browser.idl',
    )


if __name__ == '__main__':
  unittest.main()
