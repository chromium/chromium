#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import web_idl_schema

from web_idl_schema import SchemaCompilerError

# Helper functions for fetching specific parts of a processed API schema
# dictionary.


def getFunction(schema: dict, name: str) -> dict:
  """Gets the function dictionary with the specified name from the schema.

  Args:
    schema: The processed API schema dictionary to look for the function in.
    name: The name of the function to look for.

  Returns:
    The dictionary for the function with the specified name.

  Raises:
    KeyError: If the given function name was not found in the list of functions.
  """
  for item in schema['functions']:
    if item['name'] == name:
      return item
  raise KeyError('Could not find "function" with name "%s" in schema' % name)


def getType(schema: dict, name: str) -> dict:
  """Gets the custom type dictionary with the specified name from the schema.

  Args:
    schema: The processed API schema dictionary to look for the type in.
    name: The name of the custom type to look for.

  Returns:
    The dictionary for the custom type with the specified name.

  Raises:
    KeyError: If the given type name was not found in the list of types.
  """
  for item in schema['types']:
    if item['id'] == name:
      return item
  raise KeyError('Could not find "type" with id "%s" in schema' % name)


def getFunctionReturn(schema: dict, name: str) -> dict:
  """Gets the return dictionary for the function with the specified name.

  Args:
    schema: The processed API schema dictionary to look for the function in.
    name: The name of the function to get the return value from.

  Returns:
    The dictionary representing the return for the specified function name if it
    has a return, otherwise None if it does not.
  """
  function = getFunction(schema, name)
  return function.get('returns', None)


def getFunctionAsyncReturn(schema: dict, name: str) -> dict:
  """Gets the async return dictionary for the function with the specified name.

  Args:
    schema: The processed API schema dictionary to look for the function in.
    name: The name of the function to get the async return value from.

  Returns:
    The dictionary representing the async return for the function with the
    specified name if it has one, otherwise None if it does not.
  """
  function = getFunction(schema, name)
  return function.get('returns_async', None)


def getFunctionParameters(schema: dict, name: str) -> dict:
  """Gets the list of parameters for the function with the specified name.

  Args:
    schema: The processed API schema dictionary to look for the function in.
    name: The name of the function to get the parameters list from.

  Returns:
    The list of dictionaries representing the function parameters for the
    function with the specified name if it has any, otherwise None if it does
    not.
  """
  # TODO(crbug.com/340297705): All functions should have the 'parameters' key,
  # so we shouldn't have a None fallback and just raise a KeyError if it isn't
  # present.
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
        getFunctionReturn(schema, 'returnsVoid'),
    )
    self.assertEqual(
        {
            'name': 'returnsBoolean',
            'type': 'boolean'
        },
        getFunctionReturn(schema, 'returnsBoolean'),
    )
    self.assertEqual(
        {
            'name': 'returnsDouble',
            'type': 'number'
        },
        getFunctionReturn(schema, 'returnsDouble'),
    )
    self.assertEqual(
        {
            'name': 'returnsLong',
            'type': 'integer'
        },
        getFunctionReturn(schema, 'returnsLong'),
    )
    self.assertEqual(
        {
            'name': 'returnsDOMString',
            'type': 'string'
        },
        getFunctionReturn(schema, 'returnsDOMString'),
    )
    self.assertEqual({
        'name': 'returnsCustomType',
        '$ref': 'ExampleType'
    }, getFunctionReturn(schema, 'returnsCustomType'))

  def testPromiseBasedReturn(self):
    schema = self.idl_basics
    self.assertEqual(
        {
            'name': 'callback',
            'parameters': [{
                'type': 'string'
            }],
            'type': 'promise'
        }, getFunctionAsyncReturn(schema, 'stringPromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'parameters': [{
                'optional': True,
                'type': 'string'
            }],
            'type': 'promise'
        }, getFunctionAsyncReturn(schema, 'nullablePromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'parameters': [{
                '$ref': 'ExampleType'
            }],
            'type': 'promise'
        }, getFunctionAsyncReturn(schema, 'customTypePromiseReturn'))
    self.assertEqual({
        'name': 'callback',
        'parameters': [],
        'type': 'promise'
    }, getFunctionAsyncReturn(schema, 'undefinedPromiseReturn'))



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

  # Tests that a top level API comment is processed into a description
  # attribute, with HTML paragraph nodes added due to the blank commented line.
  def testApiDescriptionComment(self):
    schema = self.idl_basics
    expected_description = (
        '<p>This comment is an example of a top level API description, which'
        ' will be extracted and added to the processed python dictionary as a'
        ' description.</p><p>Note: All comment lines preceding the thing they'
        ' are attached to will be part of the description, until a blank new'
        ' line or non-comment is reached.</p>')
    self.assertEqual(expected_description, schema['description'])

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
