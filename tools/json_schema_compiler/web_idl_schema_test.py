#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import web_idl_schema
from web_idl_schema import SchemaCompilerError


def getFunction(schema, name):
  for item in schema['functions']:
    if item['name'] == name:
      return item
  raise KeyError('Could not find "function" with name "%s" in schema' % name)


def getType(schema, name):
  for item in schema['types']:
    if item['id'] == name:
      return item
  raise KeyError('Could not find "type" with id "%s" in schema' % name)


def getReturns(schema, name):
  function = getFunction(schema, name)
  return function.get('returns', None)


def getFunctionParameters(schema, name):
  function = getFunction(schema, name)
  return function.get('parameters', None)


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
        {
            'name': 'returnsBoolean',
            'type': 'boolean'
        },
        getReturns(schema, 'returnsBoolean'),
    )
    self.assertEqual(
        {
            'name': 'returnsDouble',
            'type': 'number'
        },
        getReturns(schema, 'returnsDouble'),
    )
    self.assertEqual(
        {
            'name': 'returnsLong',
            'type': 'integer'
        },
        getReturns(schema, 'returnsLong'),
    )
    self.assertEqual(
        {
            'name': 'returnsDOMString',
            'type': 'string'
        },
        getReturns(schema, 'returnsDOMString'),
    )
    self.assertEqual({
        'name': 'returnsCustomType',
        '$ref': 'ExampleType'
    }, getReturns(schema, 'returnsCustomType'))

  # Tests function parameters are processed as expected.
  def testFunctionParameters(self):
    schema = self.idl_basics
    # A function with no arguments has an empty array on the "parameters" key.
    self.assertEqual([], getFunctionParameters(schema, 'takesNoArguments'))

    self.assertEqual([{
        'name': 'stringArgument',
        'type': 'string'
    }], getFunctionParameters(schema, 'takesDOMString'))
    self.assertEqual([{
        'name': 'optionalBoolean',
        'optional': True,
        'type': 'boolean'
    }], getFunctionParameters(schema, 'takesOptionalBoolean'))
    self.assertEqual([{
        'name': 'argument1',
        'type': 'string'
    }, {
        'name': 'argument2',
        'optional': True,
        'type': 'number'
    }], getFunctionParameters(schema, 'takesMultipleArguments'))
    self.assertEqual([{
        'name': 'first',
        'type': 'string'
    }, {
        'name': 'optionalInner',
        'optional': True,
        'type': 'string'
    }, {
        'name': 'last',
        'type': 'string'
    }], getFunctionParameters(schema, 'takesOptionalInnerArgument'))
    self.assertEqual([{
        'name': 'customTypeArgument',
        '$ref': 'ExampleType'
    }], getFunctionParameters(schema, 'takesCustomType'))
    self.assertEqual([{
        'name': 'optionalCustomTypeArgument',
        'optional': True,
        '$ref': 'ExampleType'
    }], getFunctionParameters(schema, 'takesOptionalCustomType'))


  # Tests that Dictionaries defined on the top level of the IDL file are
  # processed into types on the resulting namespace.
  def testApiTypesOnNamespace(self):
    schema = self.idl_basics
    self.assertEqual(
        {
            'id': 'ExampleType',
            'properties': {
                'someString': {
                    'name': 'someString',
                    'type': 'string'
                },
                'someNumber': {
                    'name': 'someNumber',
                    'type': 'number'
                },
                'optionalBoolean': {
                    'name': 'optionalBoolean',
                    'type': 'boolean',
                    'optional': True
                }
            },
            'type': 'object'
        },
        getType(schema, 'ExampleType'),
    )

  # Tests that if the nodoc extended attribute is not specified on the API
  # interface the related attribute is set to false after processing.
  def testNodocUnspecifiedOnNamespace(self):
    self.assertFalse(self.idl_basics['nodoc'])

  # TODO(crbug.com/340297705): This will eventually be relaxed when adding
  # support for shared types to the new parser.
  def testMissingBrowserInterface(self):
    expected_error_regex = (
        '.* File\(test\/web_idl\/missing_browser_interface.idl\): Required'
        ' partial Browser interface not found in schema\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_browser_interface.idl',
    )

  # Tests that having a Browser interface on an API definition with no attribute
  # throws an error.
  def testMissingAttributeOnBrowser(self):
    expected_error_regex = (
        '.* Interface\(Browser\): The partial Browser interface should have'
        ' exactly one attribute for the name the API will be exposed under\.')
    self.assertRaisesRegex(
        Exception,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_attribute_on_browser.idl',
    )

  # Tests that using a valid basic WebIDL type with a "name" the schema compiler
  # doesn't support yet throws an error.
  def testUnsupportedBasicType(self):
    expected_error_regex = (
        '.* PrimitiveType\(float\): Unsupported basic type found when'
        ' processing type\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/unsupported_basic_type.idl',
    )

  # Tests that using a valid WebIDL type with a node "class" the schema compiler
  # doesn't support yet throws an error.
  def testUnsupportedTypeClass(self):
    expected_error_regex = (
        '.* Any\(\): Unsupported type class when processing type\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/unsupported_type_class.idl',
    )

  # Tests that an API interface that uses the nodoc extended attribute has the
  # related nodoc attribute set to true after processing.
  def testNoDocOnNamespace(self):
    nodoc_schema = web_idl_schema.Load('test/web_idl/nodoc_on_namespace.idl')
    self.assertEqual(1, len(nodoc_schema))
    self.assertEqual('nodocAPI', nodoc_schema[0]['namespace'])
    self.assertTrue(nodoc_schema[0]['nodoc'])


if __name__ == '__main__':
  unittest.main()
