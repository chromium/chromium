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


def getProperty(schema: dict, name: str) -> dict:
  """Gets the property dictionary with the specified name from the schema.

  Args:
    schema: The processed API schema dictionary to look for the property in.
    name: The name of the property to look for.

  Returns:
    The dictionary for the property with the specified name.
  """
  return schema['properties'][name]


def getEvent(schema: dict, name: str) -> dict:
  """Gets the event dictionary with the specified name from the schema.

  Args:
    schema: The processed API schema dictionary to look for the event in.
    name: The name of the event to look for.

  Returns:
    The dictionary for the event with the specified name.

  Raises:
    KeyError: If the given event name was not found in the list of events.
  """
  for item in schema['events']:
    if item['name'] == name:
      return item
  raise KeyError('Could not find "event" with name "%s" in schema' % name)


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

  def testFunctionBasics(self):
    function = getFunction(self.idl_basics, 'returnsUndefined')
    self.assertEqual('returnsUndefined', function.get('name'))
    self.assertEqual([], function.get('parameters'))
    self.assertEqual('function', function.get('type'))

  def testFunctionReturnTypes(self):
    schema = self.idl_basics
    # Test basic types.
    self.assertEqual(
        None,
        getFunctionReturn(schema, 'returnsUndefined'),
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
    self.assertEqual(
        {
            'name': 'returnsDOMStringSequence',
            'type': 'array',
            'items': {
                'type': 'string'
            }
        }, getFunctionReturn(schema, 'returnsDOMStringSequence'))
    self.assertEqual(
        {
            'name': 'returnsCustomTypeSequence',
            'type': 'array',
            'items': {
                '$ref': 'ExampleType'
            }
        }, getFunctionReturn(schema, 'returnsCustomTypeSequence'))

  def testPromiseBasedReturn(self):
    schema = self.idl_basics
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'type': 'string'
            }],
        }, getFunctionAsyncReturn(schema, 'stringPromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'optional': True,
                'type': 'string'
            }],
        }, getFunctionAsyncReturn(schema, 'nullablePromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                '$ref': 'ExampleType'
            }],
        }, getFunctionAsyncReturn(schema, 'customTypePromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [],
        }, getFunctionAsyncReturn(schema, 'undefinedPromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'type': 'array',
                'items': {
                    'type': 'integer'
                }
            }],
        }, getFunctionAsyncReturn(schema, 'longSequencePromiseReturn'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'type': 'array',
                'items': {
                    '$ref': 'ExampleType'
                }
            }],
        }, getFunctionAsyncReturn(schema, 'customTypeSequencePromiseReturn'))

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
        'name': 'sequenceArgument',
        'type': 'array',
        'items': {
            'type': 'boolean'
        }
    }], getFunctionParameters(schema, 'takesSequenceArgument'))
    self.assertEqual([{
        'name': 'optionalSequenceArgument',
        'type': 'array',
        'optional': True,
        'items': {
            'type': 'boolean'
        }
    }], getFunctionParameters(schema, 'takesOptionalSequenceArgument'))
    self.assertEqual([{
        'name': 'customTypeArgument',
        '$ref': 'ExampleType'
    }], getFunctionParameters(schema, 'takesCustomType'))
    self.assertEqual([{
        'name': 'optionalCustomTypeArgument',
        'optional': True,
        '$ref': 'ExampleType'
    }], getFunctionParameters(schema, 'takesOptionalCustomType'))
    self.assertEqual([{
        'name': 'enumArgument',
        '$ref': 'EnumType'
    }], getFunctionParameters(schema, 'takesEnum'))

  # Tests function descriptions are processed as expected.
  def testFunctionDescriptions(self):
    schema = self.idl_basics
    # A function without a preceding comment has no 'description' key.
    self.assertTrue('description' not in getFunction(schema, 'noDescription'))

    # Basic single and multi-line function comments.
    self.assertEqual(
        'One line description.',
        getFunction(schema, 'oneLineDescription').get('description'))
    self.assertEqual(
        'Multi line description. Split over. Multiple lines.',
        getFunction(schema, 'multiLineDescription').get('description'))
    self.assertEqual(
        '<p>Paragraphed description.</p><p>With blank comment line for'
        ' paragraph tags.</p>',
        getFunction(schema, 'paragraphedDescription').get('description'))

    # Function with parameter comments.
    function = getFunction(schema, 'parameterComments')
    self.assertEqual('This function has parameter comments.',
                     function.get('description'))
    function_parameters = getFunctionParameters(schema, 'parameterComments')
    self.assertEqual(2, len(function_parameters))
    self.assertEqual(
        {
            'description':
            ('This comment about the argument is split across multiple lines'
             ' and contains <em>HTML tags</em>.'),
            'name':
            'arg1',
            'type':
            'boolean',
        },
        function_parameters[0],
    )
    self.assertEqual(
        {
            'description': 'This second argument uses a custom type.',
            'name': 'arg2',
            '$ref': 'ExampleType'
        }, function_parameters[1])

    # Basic descriptions on a promise returning async function.
    promise_function = getFunction(schema, 'describedPromiseReturn')
    self.assertEqual(
        ('Promise returning function, with a comment that provides the name and'
         ' description of the value the promise resolves to.'),
        promise_function.get('description'))
    promise_function_parameters = getFunctionParameters(
        schema, 'describedPromiseReturn')
    self.assertEqual(1, len(promise_function_parameters))
    self.assertEqual(
        {
            'description': 'This is a normal argument comment.',
            'name': 'arg1',
            'type': 'boolean',
        },
        promise_function_parameters[0],
    )
    promise_function_async_return = getFunctionAsyncReturn(
        schema, 'describedPromiseReturn')
    self.assertEqual(
        {
            'name':
            'callback',
            'optional':
            True,
            'description':
            'General description for the promise return.',
            'parameters': [{
                '$ref':
                'ExampleType',
                'name':
                'returnValueName',
                'description':
                ('A description for the value the promise resolves to: with'
                 ' an extra colon for good measure.'),
            }],
        }, promise_function_async_return)

    # Promise returning function with just a name for the promise value and no
    # further description.
    named_promise_function_async_return = getFunctionAsyncReturn(
        schema, 'namedPromiseReturn')
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'type': 'boolean',
                'name': 'justAName'
            }],
        }, named_promise_function_async_return)

    # Function with a return and simple comment describing it.
    return_function = getFunction(schema, 'describedReturnFunction')
    self.assertEqual(
        'General function description for the describedReturnFunction.',
        return_function.get('description'))
    return_function_returns_value = getFunctionReturn(
        schema, 'describedReturnFunction')
    self.assertEqual('Description for the returns object itself.',
                     return_function_returns_value.get('description'))

  # Tests that API events are processed as expected.
  def testEvents(self):
    schema = self.idl_basics

    event_one = getEvent(schema, 'onTestOne')
    # This is a bit of a tautology for now, as getEvent() uses name to retrieve
    # the object and raises a KeyError if it is not found.
    self.assertEqual('onTestOne', event_one.get('name'))
    self.assertEqual('function', event_one.get('type'))
    self.assertEqual(
        'Comment that acts as a description for onTestOne. Parameter specific'
        ' comments are down below before the associated callback definition.',
        event_one.get('description'))
    self.assertEqual(
        [{
            'name': 'argument1',
            'type': 'string',
            'description': 'Parameter description for argument1.'
        }, {
            'name': 'argument2',
            'optional': True,
            'type': 'number',
            'description': 'Another description, this time for argment2.'
        }], event_one['parameters'])

    event_two = getEvent(schema, 'onTestTwo')
    self.assertEqual('onTestTwo', event_two.get('name'))
    self.assertEqual('function', event_two.get('type'))
    self.assertEqual('Comment for onTestTwo.', event_two.get('description'))
    self.assertEqual(
        [{
            'name': 'customType',
            '$ref': 'ExampleType',
            'description': 'An ExampleType passed to the event listener.'
        }], event_two['parameters'])

  # Tests that Dictionaries and Enums defined on the top level of the IDL file
  # are processed into types on the resulting namespace.
  def testApiTypesOnNamespace(self):
    schema = self.idl_basics
    custom_type = getType(schema, 'ExampleType')
    self.assertEqual('ExampleType', custom_type['id'])
    self.assertEqual('object', custom_type['type'])
    self.assertEqual(
        {
            'name': 'someString',
            'type': 'string',
            'description':
            'Attribute comment attached to ExampleType.someString.'
        }, custom_type['properties']['someString'])
    self.assertEqual(
        {
            'name': 'someNumber',
            'type': 'number',
            'description':
            'Comment where <var>someNumber</var> has some markup.'
        }, custom_type['properties']['someNumber'])
    # TODO(crbug.com/379052294): using HTML comments like this is a bit of a
    # hack to allow us to add comments in IDL files (e.g. for TODOs) and to not
    # have them end up on the documentation site. We should probably just filter
    # them out during compilation.
    self.assertEqual(
        {
            'name':
            'optionalBoolean',
            'type':
            'boolean',
            'optional':
            True,
            'description':
            'Comment with HTML comment. <!-- Which should get through -->'
        }, custom_type['properties']['optionalBoolean'])
    self.assertEqual(
        {
            'name': 'booleanSequence',
            'type': 'array',
            'items': {
                'type': 'boolean'
            },
            'description': 'Comment on sequence type.',
        }, custom_type['properties']['booleanSequence'])

    enum_expected = {
        'enum': [{
            'name': 'name1',
            'description': 'Comment1.'
        }, {
            'name': 'name2'
        }],
        'description': 'Enum description.',
        'type': 'string',
        'id': 'EnumType'
    }
    self.assertEqual(enum_expected, getType(schema, 'EnumType'))

    expected_type_with_function = {
        'name': 'callbackMember',
        'type': 'function',
        'parameters': [{
            'name': 'stringArgument',
            'type': 'string'
        }]
    }
    self.assertEqual(
        expected_type_with_function,
        getType(schema,
                'DictionaryWithCallbackMember')['properties']['callbackMember'])

  # Tests that a schema that references a custom type that has not been defined
  # causes an error to be thrown.
  # TODO(crbug.com/450443604): This will likely have to change when adding
  # support for shared types between schema files in WebIDL.
  def testCustomTypeNotFound(self):
    expected_error_regex = (
        r'.* Typeref\(MissingType\): Could not find definition of referenced'
        r' type "MissingType" for node.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/custom_type_not_found.idl',
    )

  # Tests that a schema referencing a typeref style type whose definition is not
  # one of the valid IDL node classes (Dictionary, Enum, Callback) causes an
  # error to be thrown.
  def testInvalidTyperefType(self):
    expected_error_regex = (
        r'.* Typeref\(OnTestEvent\): Found a Typeref node referencing a node of'
        r' type "Interface", but we only support Typerefs that reference'
        r' Dictionary, Enum or Callback class nodes.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/invalid_typeref_type.idl',
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

  # Tests that constants defined on an API Interface are processed into the
  # 'properties' object.
  def testConstantProperties(self):
    schema = self.idl_basics
    # Properties are an ordered dict, so ordering matches order in the IDL file.
    self.assertEqual(
        [
            'CONSTANT_LONG',
            'CONSTANT_DOUBLE',
            'CONSTANT_STRING',
            'DESCRIBED_CONSTANT',
        ],
        list(schema['properties'].keys()),
    )
    self.assertEqual({
        'type': 'integer',
        'value': 39
    }, getProperty(schema, 'CONSTANT_LONG'))
    self.assertEqual({
        'type': 'number',
        'value': 3.9
    }, getProperty(schema, 'CONSTANT_DOUBLE'))
    self.assertEqual({
        'type': 'string',
        'value': 'Foo'
    }, getProperty(schema, 'CONSTANT_STRING'))
    self.assertEqual(
        {
            'type': 'integer',
            'value': 9,
            'description': 'Comment on a constant property with a value.',
        }, getProperty(schema, 'DESCRIBED_CONSTANT'))

  # Tests that a const DOMString defined on an API Interface which is missing
  # the StringValue extended attribute will throw an error. It's unfortunate
  # that we need to hack this in with an extended attribute, but WebIDL does not
  # support specifying an actual string for the value of a const.
  def testConstStringMissingExtendedAttribute(self):
    expected_error_regex = (
        r'.* Const\(foo\): If using a const of type DOMString, you must specify'
        r' the extended attribute "StringValue" for the value.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/const_string_missing_extended_attribute.idl',
    )

  # Tests that if the nodoc extended attribute is not specified on the API
  # interface the related attribute is set to false after processing.
  def testNodocUnspecifiedOnNamespace(self):
    self.assertFalse(self.idl_basics['nodoc'])

  # Test loading a schema for an API with a nested namespace comes through with
  # a dot separator in its name after processing (nested.example).
  def testNestedNamespaceName(self):
    idl = web_idl_schema.Load('test/web_idl/nested_namespace.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    self.assertEqual('nested.example', schema['namespace'])
    self.assertEqual(
        'Schema to test nested namespacee names types in IDL (API names with a'
        ' dot in them). API should end up on `<Browser>.nested.example`.',
        schema['description'],
    )

  # Similar to above, but testing a triple nested namespace with 2 dots
  # (multi.nested.example).
  def testMultiNestedNamespaceName(self):
    idl = web_idl_schema.Load('test/web_idl/multi_nested_namespace.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    self.assertEqual('multi.nested.example', schema['namespace'])
    self.assertEqual(
        'Schema to test triple nested namespacee names types in IDL (API names'
        ' with 2 dots in them). API should end up on `<Browser>.multi.nested.'
        'example`.',
        schema['description'],
    )

  # TODO(crbug.com/340297705): This will eventually be relaxed when adding
  # support for shared types to the new parser.
  def testMissingBrowserInterfaceError(self):
    expected_error_regex = (
        r'.* File\(test\/web_idl\/missing_browser_interface.idl\): Required'
        r' partial Browser interface not found in schema\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_browser_interface.idl',
    )

  # Tests that having a Browser interface on an API definition with no attribute
  # throws an error.
  def testMissingAttributeOnBrowserError(self):
    expected_error_regex = (
        r'.* Interface\(Browser\): The partial Browser interface should have'
        r' exactly one attribute for the name the API will be exposed under\.')
    self.assertRaisesRegex(
        Exception,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_attribute_on_browser.idl',
    )

  # Tests that using a valid basic WebIDL type with a "name" the schema compiler
  # doesn't support yet throws an error.
  def testUnsupportedBasicTypeError(self):
    expected_error_regex = (
        r'.* PrimitiveType\(float\): Unsupported basic type found when'
        r' processing type\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/unsupported_basic_type.idl',
    )

  # Tests that using a valid WebIDL type with a node "class" the schema compiler
  # doesn't support yet throws an error.
  def testUnsupportedTypeClassError(self):
    expected_error_regex = (
        r'.* FrozenArray\(\): Unsupported type class when processing type\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/unsupported_type_class.idl',
    )

  # Tests that an event trying to say it uses an Interface that is not defined
  # in the IDL file will throw an error. This is largely in place to help catch
  # spelling mistakes in event names or forgetting to add the Interface
  # definition.
  def testMissingEventInterface(self):
    expected_error_regex = (
        r'.* Error processing node Attribute\(onTestTwo\): Could not find'
        r' Interface definition for event\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_event_interface.idl',
    )

  # Various tests that ensure validation on event interface definitions.
  # Specifically checks that not defining any of the required add/remove/has
  # Operations or forgetting the ExtensionEvent inheritance will throw an error.
  def testMissingEventInheritance(self):
    expected_error_regex = (
        r'.* Error processing node Interface\(OnMissingInheritanceEvent\):'
        r' Event Interface missing ExtensionEvent Inheritance.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_event_inheritance.idl',
    )

  def testMissingEventAddListener(self):
    expected_error_regex = (
        r'.* Error processing node Interface\(OnMissingAddListenerEvent\):'
        r' Event Interface missing addListener Operation definition.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_event_add_listener.idl',
    )

  def testMissingEventRemoveListener(self):
    expected_error_regex = (
        r'.* Error processing node Interface\(OnMissingRemoveListenerEvent\):'
        r' Event Interface missing removeListener Operation definition.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_event_remove_listener.idl',
    )

  def testMissingEventHasListener(self):
    expected_error_regex = (
        r'.* Error processing node Interface\(OnMissingHasListenerEvent\):'
        r' Event Interface missing hasListener Operation definition.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/missing_event_has_listener.idl',
    )

  # Tests that if description parsing from file comments reaches the top of the
  # file, a schema compiler error is thrown (as the top of the file should
  # always be copyright lines and not part of the description).
  def testDocumentationCommentReachedTopOfFileError(self):
    expected_error_regex = (
        r'.* Reached top of file when trying to parse description from file'
        r' comment. Make sure there is a blank line before the comment.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/documentation_comment_top_of_file.idl',
    )

  # Tests that usage of the 'void' type will result in a schema compiler error.
  # 'void' has been deprecated and 'undefined' should be used instead.
  def testVoidUsageTriggersError(self):
    expected_error_regex = (
        r'Error processing node PrimitiveType\(void\): Usage of "void" in IDL'
        r' is deprecated, use "Undefined" instead.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/void_unsupported.idl',
    )

  # Tests that the nodoc extended attribute used in various places gets the
  # related attribute set to True after processing.
  def testNoDocExtendedAttribute(self):
    idl = web_idl_schema.Load('test/web_idl/nodoc_examples.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    # Top level namespace:
    self.assertEqual('nodocAPI', schema['namespace'])
    self.assertTrue(schema['nodoc'])
    # Also ensure the description comes through correctly on the node with
    # 'nodoc' as an extended attribute.
    self.assertEqual(
        'The nodoc API. This exists to demonstrate a variety of nodoc extended'
        ' attribute usage.',
        schema['description'],
    )

    # Enums:
    nodoc_enum = getType(schema, 'EnumWithNoDoc')
    self.assertTrue(nodoc_enum['nodoc'])
    normal_enum = getType(schema, 'NormalEnum')
    self.assertFalse(hasattr(normal_enum, 'nodoc'))

    # Dictionaries:
    nodoc_dict = getType(schema, 'DictionaryWithNoDoc')
    self.assertTrue(nodoc_dict['nodoc'])
    normal_dict = getType(schema, 'NormalDictionary')
    self.assertFalse(hasattr(normal_dict, 'nodoc'))

    # Dictionary members:
    nodoc_dict_member = getType(schema, 'DictionaryWithNoDocMember')
    self.assertTrue(nodoc_dict_member['properties']['nodocMember']['nodoc'])
    self.assertFalse(
        hasattr(nodoc_dict_member['properties']['normalMember'], 'nodoc'))

    # Functions:
    nodoc_function = getFunction(schema, 'functionWithNoDoc')
    self.assertTrue(nodoc_function['nodoc'])
    normal_function = getFunction(schema, 'normalFunction')
    self.assertFalse(hasattr(normal_function, 'nodoc'))

    # Events:
    nodoc_event = getEvent(schema, 'noDocEvent')
    self.assertTrue(nodoc_event['nodoc'])
    normal_event = getEvent(schema, 'normalEvent')
    self.assertFalse(hasattr(normal_event, 'nodoc'))

    # Properties:
    nodoc_property = getProperty(schema, 'PROPERTY_WITH_NODOC')
    self.assertTrue(nodoc_property['nodoc'])
    normal_property = getProperty(schema, 'NORMAL_PROPERTY')
    self.assertFalse(hasattr(normal_property, 'nodoc'))

  # Tests that the nocompile extended attribute used in various places gets the
  # related attribute set to True after processing.
  # Note: Practically with how nocompile works during actual schema compilation
  # this schema doesn't really make sense, as the top level schema node would be
  # deleted, making all the other 'nocompile's redundant. However we can still
  # use this schema to check that the attribute is set correctly on each node
  # after just the parsing and processing steps (before compilation).
  def testNoCompileExtendedAttribute(self):
    idl = web_idl_schema.Load('test/web_idl/nocompile_examples.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    # Top level namespace:
    self.assertEqual('nocompileAPI', schema['namespace'])
    self.assertTrue(schema['nocompile'])

    # Enums:
    nocompile_enum = getType(schema, 'EnumWithNoCompile')
    self.assertTrue(nocompile_enum['nocompile'])
    normal_enum = getType(schema, 'NormalEnum')
    self.assertFalse(hasattr(normal_enum, 'nocompile'))

    # Dictionaries:
    nocompile_dict = getType(schema, 'DictionaryWithNoCompile')
    self.assertTrue(nocompile_dict['nocompile'])
    normal_dict = getType(schema, 'NormalDictionary')
    self.assertFalse(hasattr(normal_dict, 'nocompile'))

    # Dictionary members:
    nocompile_dict_member = getType(schema, 'DictionaryWithNoCompileMember')
    self.assertTrue(
        nocompile_dict_member['properties']['nocompileMember']['nocompile'])
    self.assertFalse(
        hasattr(nocompile_dict_member['properties']['normalMember'],
                'nocompile'))

    # Functions:
    nocompile_function = getFunction(schema, 'functionWithNoCompile')
    self.assertTrue(nocompile_function['nocompile'])
    normal_function = getFunction(schema, 'normalFunction')
    self.assertFalse(hasattr(normal_function, 'nocompile'))

    # Events:
    nocompile_event = getEvent(schema, 'noCompileEvent')
    self.assertTrue(nocompile_event['nocompile'])
    normal_event = getEvent(schema, 'normalEvent')
    self.assertFalse(hasattr(normal_event, 'nocompile'))

    # Properties:
    nocompile_property = getProperty(schema, 'PROPERTY_WITH_NOCOMPILE')
    self.assertTrue(nocompile_property['nocompile'])
    normal_property = getProperty(schema, 'NORMAL_PROPERTY')
    self.assertFalse(hasattr(normal_property, 'nocompile'))

  # Tests that the deprecated extended attribute used in various places get the
  # related attribute set to the provided string after processing.
  # TODO(crbug.com/340297705): Enum values are not allowed to have extended
  # attributes preceding them, so we need to find some other way to mark a
  # specific enum value as deprecated.
  def testDeprecatedExtendedAttribute(self):
    idl = web_idl_schema.Load('test/web_idl/deprecated_examples.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    # Top level Namespace:
    self.assertEqual('This API is deprecated', schema['deprecated'])

    # Enums:
    deprecated_enum = getType(schema, 'DeprecatedEnum')
    self.assertEqual('This enum is deprecated', deprecated_enum['deprecated'])

    # Dictionaries:
    deprecated_dict = getType(schema, 'DeprecatedDictionary')
    self.assertEqual('This dict is deprecated', deprecated_dict['deprecated'])

    # Dictionary members:
    deprecated_dict_member = getType(schema, 'DictionaryWithDeprecatedMember')
    self.assertEqual(
        'This dict member is deprecated',
        deprecated_dict_member['properties']['deprecatedMember']['deprecated'])

    # Functions:
    deprecated_function = getFunction(schema, 'deprecatedFunction')
    self.assertEqual(
        'This function is deprecated and it has such a long message that\n  it'
        ' requires line wrapping',
        deprecated_function['deprecated'],
    )

    # Events:
    deprecated_event = getEvent(schema, 'onDeprecatedEvent')
    self.assertEqual('This event is deprecated', deprecated_event['deprecated'])

    # Properties:
    deprecated_property = getProperty(schema, 'DEPRECATED_PROPERTY')
    self.assertEqual('This property is deprecated',
                     deprecated_property['deprecated'])

  # Tests that a function defined with the requiredCallback extended attribute
  # does not have the returns_async field marked as optional after processing.
  # Note: These are only relevant to contexts which don't support promise based
  # calls, or for specific functions which still do not support promises.
  def testRequiredCallbackFunction(self):
    idl = web_idl_schema.Load('test/web_idl/required_callback_function.idl')
    self.assertEqual(1, len(idl))
    self.assertEqual(
        {
            'name': 'callback',
            'parameters': [{
                'type': 'string'
            }],
        }, getFunctionAsyncReturn(idl[0], 'requiredCallbackFunction'))
    self.assertEqual(
        {
            'name': 'callback',
            'optional': True,
            'parameters': [{
                'type': 'string'
            }],
        }, getFunctionAsyncReturn(idl[0], 'notRequiredCallbackFunction'))

  # Tests that extended attributes being listed on the the line previous to a
  # node come through correctly and don't throw off and associated descriptions.
  def testPreviousLineExtendedAttributes(self):
    idl = web_idl_schema.Load('test/web_idl/preceding_extended_attributes.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]
    self.assertEqual('precedingExtendedAttributes', schema['namespace'])
    self.assertTrue(schema['nodoc'])
    self.assertEqual(
        'Comment on a schema that has extended attributes on a previous line.',
        schema['description'],
    )

    function = getFunction(schema, 'functionExample')
    self.assertEqual('Description on a function.', function.get('description'))
    async_return = getFunctionAsyncReturn(schema, 'functionExample')
    # The extended attribute on the function causes 'optional': True to not be
    # present on the async return.
    self.assertNotIn('optional', async_return)
    self.assertEqual('Promise return description.',
                     async_return.get('description'))

  # Tests that an API interface with the platforms extended attribute has these
  # values in a platforms attribute after processing.
  def testAllPlatformsOnNamespace(self):
    platforms_schema = web_idl_schema.Load(
        'test/web_idl/all_platforms_on_namespace.idl')
    self.assertEqual(1, len(platforms_schema))
    self.assertEqual('allPlatformsAPI', platforms_schema[0]['namespace'])
    expected = ['chromeos', 'desktop_android', 'linux', 'mac', 'win']
    self.assertEqual(expected, platforms_schema[0]['platforms'])

  # Tests that an API interface and a function definition with chromeos listed
  # in the platforms extended attribute has the associated attribute set after
  # processing.
  # Note: Platform restrictions should generally be defined using Extension
  # Features Files (see: chrome/common/extensions/api/_features.md), but for
  # legacy reasons we have to allow this extended attribute on namespace and
  # function definitions.
  def testPlatformsExtendedAttribute(self):
    idl = web_idl_schema.Load('test/web_idl/platforms_examples.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    self.assertEqual('chromeOSPlatformsAPI', schema['namespace'])
    expected = ['chromeos']
    self.assertEqual(expected, schema['platforms'])

    platforms_function = getFunction(schema, 'chromeOSOnlyFunction')
    self.assertEqual(expected, platforms_function['platforms'])

    normal_function = getFunction(schema, 'normalFunction')
    self.assertFalse(hasattr(normal_function, 'platforms'))


  # Tests that the 'implemented_in' extended attribute on an interface
  # definition is copied into the resulting namespace after processing.
  def testImplementedInExtendedAttribute(self):
    idl = web_idl_schema.Load('test/web_idl/implemented_in.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    self.assertEqual('implementedInAPI', schema['namespace'])
    self.assertEqual('path/to/implementation.h',
                     schema['compiler_options']['implemented_in'])

  # Tests that an API interface using the 'generate_error_messages' extended
  # attribute has the associated attribute set to true after processing.
  def testGenerateErrorMessages(self):
    idl = web_idl_schema.Load('test/web_idl/generate_error_messages.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    self.assertEqual('generateErrorMessagesAPI', schema['namespace'])
    self.assertTrue(schema['compiler_options']['generate_error_messages'])

  # Tests a variety of default values that are set on an API namespace when they
  # are not specified in the source IDL file.
  def testNonSpecifiedDefaultValues(self):
    defaults_schema = web_idl_schema.Load('test/web_idl/defaults.idl')[0]
    self.assertEqual(
        {
            'compiler_options': {},
            'deprecated': None,
            'description': '',
            'events': [],
            'functions': [],
            'manifest_keys': None,
            'namespace': 'defaultsOnlyWebIdl',
            'nodoc': False,
            'platforms': None,
            'properties': {},
            'types': [],
        }, defaults_schema)

  # Tests that Enum and Dictionary types defined in a schema file retain their
  # ordering in the resulting processed API object.
  def testEnumAndTypeOrdering(self):
    idl = web_idl_schema.Load('test/web_idl/enum_and_type_ordering.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]
    # Types are interleaved in the schema.
    types = schema['types']
    self.assertEqual('ExampleDictOne', types[0]['id'])
    self.assertEqual('EnumTypeOne', types[1]['id'])
    self.assertEqual('ExampleDictTwo', types[2]['id'])
    self.assertEqual('EnumTypeTwo', types[3]['id'])

  # Tests various 'any' and 'object' types on Dictionaries.
  def testObjectTypes(self):
    idl = web_idl_schema.Load('test/web_idl/object_types.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    object_dict = getType(schema, 'ObjectDict')
    self.assertEqual('object', object_dict['type'])
    self.assertEqual(
        {
            'type': 'object',
            'name': 'requiredObject',
            'additionalProperties': {
                'type': 'any'
            }
        }, object_dict['properties']['requiredObject'])
    self.assertEqual(
        {
            'type': 'object',
            'optional': True,
            'name': 'optionalObject',
            'additionalProperties': {
                'type': 'any'
            }
        }, object_dict['properties']['optionalObject'])
    self.assertEqual(
        {
            'type': 'object',
            'name': 'instanceOfObject',
            'additionalProperties': {
                'type': 'any'
            },
            'isInstanceOf': 'Blob'
        }, object_dict['properties']['instanceOfObject'])

    any_dict = getType(schema, 'AnyDict')
    self.assertEqual('object', any_dict['type'])
    self.assertEqual({
        'type': 'any',
        'name': 'requiredAny'
    }, any_dict['properties']['requiredAny'])
    self.assertEqual({
        'type': 'any',
        'optional': True,
        'name': 'optionalAny'
    }, any_dict['properties']['optionalAny'])

  # Tests 'object' and 'any' types used as function parameters.
  def testObjectFunctionParams(self):
    idl = web_idl_schema.Load('test/web_idl/object_types.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    object_params = getFunctionParameters(schema, 'objectParamFunction')
    self.assertEqual(
        {
            'type': 'object',
            'name': 'requiredObjectParam',
            'additionalProperties': {
                'type': 'any'
            }
        }, object_params[0])
    self.assertEqual(
        {
            'type': 'object',
            'optional': True,
            'name': 'optionalObjectParam',
            'additionalProperties': {
                'type': 'any'
            }
        }, object_params[1])

    any_params = getFunctionParameters(schema, 'anyParamFunction')
    self.assertEqual({'type': 'any', 'name': 'requiredAnyParam'}, any_params[0])
    self.assertEqual(
        {
            'type': 'any',
            'optional': True,
            'name': 'optionalAnyParam'
        }, any_params[1])

    instance_of_params = getFunctionParameters(schema,
                                               'instanceOfFunctionParam')
    self.assertEqual(
        {
            'type': 'object',
            'name': 'instanceOfParam',
            'additionalProperties': {
                'type': 'any'
            },
            'isInstanceOf': 'Entry'
        }, instance_of_params[0])

  # Tests various Union types on Dictionaries.
  def testUnionTypes(self):
    schema = self.idl_basics
    union_dict = getType(schema, 'UnionTypes')

    self.assertEqual(
        {
            'name':
            'stringLongOrBoolean',
            'choices': [
                {
                    'type': 'string'
                },
                {
                    'type': 'integer'
                },
                {
                    'type': 'boolean'
                },
            ],
        }, union_dict['properties']['stringLongOrBoolean'])
    self.assertEqual(
        {
            'name': 'stringOrEnum',
            'choices': [
                {
                    'type': 'string'
                },
                {
                    '$ref': 'EnumType'
                },
            ],
        }, union_dict['properties']['stringOrEnum'])
    self.assertEqual(
        {
            'name': 'optionalEnumOrString',
            'optional': True,
            'choices': [
                {
                    '$ref': 'EnumType'
                },
                {
                    'type': 'string'
                },
            ],
        }, union_dict['properties']['optionalEnumOrString'])
    self.assertEqual(
        {
            'name':
            'optionalStringOrStringSequence',
            'optional':
            True,
            'choices': [
                {
                    'type': 'string'
                },
                {
                    'type': 'array',
                    'items': {
                        'type': 'string'
                    }
                },
            ],
        }, union_dict['properties']['optionalStringOrStringSequence'])
    self.assertEqual(
        {
            'name': 'dictTypeOrLong',
            'choices': [{
                '$ref': 'ExampleType'
            }, {
                'type': 'integer'
            }]
        }, union_dict['properties']['dictTypeOrLong'])

  # Tests 'ArrayBuffer' types on Dictionaries.
  def testArrayBufferTypes(self):
    idl = web_idl_schema.Load('test/web_idl/array_buffer.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    array_buffer_dict = getType(schema, 'ArrayBufferDict')
    self.assertEqual('object', array_buffer_dict['type'])
    self.assertEqual(
        {
            'type': 'binary',
            'name': 'requiredArrayBuffer',
            'isInstanceOf': 'ArrayBuffer'
        }, array_buffer_dict['properties']['requiredArrayBuffer'])
    self.assertEqual(
        {
            'type': 'binary',
            'optional': True,
            'name': 'optionalArrayBuffer',
            'isInstanceOf': 'ArrayBuffer'
        }, array_buffer_dict['properties']['optionalArrayBuffer'])

  # Test 'ArrayBuffer' types used as function parameters.
  def testArrayBufferFunctionParams(self):
    idl = web_idl_schema.Load('test/web_idl/array_buffer.idl')
    self.assertEqual(1, len(idl))
    schema = idl[0]

    array_buffer_params = getFunctionParameters(schema,
                                                'arrayBufferParamFunction')
    self.assertEqual(
        {
            'type': 'binary',
            'name': 'requiredArrayBufferParam',
            'isInstanceOf': 'ArrayBuffer'
        }, array_buffer_params[0])
    self.assertEqual(
        {
            'type': 'binary',
            'optional': True,
            'name': 'optionalArrayBufferParam',
            'isInstanceOf': 'ArrayBuffer'
        }, array_buffer_params[1])

  # Tests Manifest keys defined on a partial 'Manifest' dictionary are
  # extracted and put into the manifest keys details and not into the Types.
  def testManifestKeys(self):
    schema = self.idl_basics
    # The 'Manifest' dictionary shouldn't get put into the custom types.
    self.assertFalse(any(obj['id'] == 'Manifest' for obj in schema['types']))
    manifest_keys = schema['manifest_keys']

    # We should have 3 manifest keys of varying types.
    self.assertEqual(['string_key', 'custom_type_key', 'union_type_key'],
                     list(manifest_keys.keys()))
    self.assertEqual(
        {
            'type': 'string',
            'name': 'string_key',
            'description': 'Description of a manifest key.'
        }, manifest_keys['string_key'])
    self.assertEqual({
        '$ref': 'ExampleType',
        'name': 'custom_type_key'
    }, manifest_keys['custom_type_key'])
    self.assertEqual(
        {
            'choices': [{
                'type': 'string'
            }, {
                'type': 'integer'
            }],
            'name': 'union_type_key'
        }, manifest_keys['union_type_key'])

  # Tests that if 'partial' is left off the 'Manifest' dictionary, we throw an
  # error.
  def testNonPartialManifestDictError(self):
    expected_error_regex = (
        r'.* Dictionary\(Manifest\): If using a "Manifest" dictionary to define'
        r' manifest keys, it must be declared "partial"\.')
    self.assertRaisesRegex(
        SchemaCompilerError,
        expected_error_regex,
        web_idl_schema.Load,
        'test/web_idl/non_partial_manifest_dict.idl',
    )


if __name__ == '__main__':
  unittest.main()
