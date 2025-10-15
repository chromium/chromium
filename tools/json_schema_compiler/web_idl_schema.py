#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib
import itertools
import json
import linecache
import os.path
import re
import sys
from abc import ABC, abstractmethod
from typing import Dict, List, Optional, NamedTuple, Union
from collections import OrderedDict

# This file is a peer to json_schema.py and idl_schema.py. Each of these files
# understands a certain format describing APIs (either JSON, old extensions IDL
# or modern WebIDL), reads files written in that format into memory, and emits
# them as a Python array of objects corresponding to those APIs, where the
# objects are formatted in a way that the JSON schema compiler understands.
# compiler.py drives which schema processor is used.
# TODO(crbug.com/340297705): Currently compiler.py only uses the other
# processors, but support for this processor will be added once it can start to
# handle full API files.

# idl_parser expects to be able to import certain files in its directory,
# so let's set things up the way it wants.
_idl_generators_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                    os.pardir, os.pardir, 'tools')
sys.path.insert(0, _idl_generators_path)
try:
  import idl_parser
  importlib.reload(idl_parser)
  from idl_parser import idl_parser, idl_lexer, idl_node
finally:
  sys.path.pop(0)

IDLNode = idl_node.IDLNode  # Used for type hints.


class SchemaCompilerError(Exception):

  def __init__(self, message: str, node: IDLNode):
    super().__init__(
        node.GetLogLine(f'Error processing node {node}: {message}'))


class UndefinedType:
  """Represents a type with no value, similar to void or undefined in IDL."""


def GetChildWithName(node: IDLNode, name: str) -> Optional[IDLNode]:
  """Gets the first child node with a given name from an IDLNode.

  Args:
    node: The IDLNode to look through the children of.
    name: The name of the node to look for.

  Returns:
    The first child found with the specified name or None if a child with that
    name was not found.
  """
  return next(
      (child for child in node.GetChildren() if child.GetName() == name), None)


def GetTypeName(node: IDLNode) -> str:
  """Gets the name of the defined IDL type from an IDLNode.

  Args:
    node: The IDLNode to return the type name from.

  Returns:
    The string representing the name given to this IDL type definition.

  Raises:
    SchemaCompilerError: If a child of class 'Type' was not found on the node.
  """
  for child_node in node.GetChildren():
    if child_node.GetClass() == 'Type':
      return child_node.GetOneOf('Typeref').GetName()
  raise SchemaCompilerError(
      'Could not find Type node when looking for Typeref name.', node)


def GetExtendedAttributes(node: IDLNode) -> Optional[List[IDLNode]]:
  """Returns the list of extended attribute nodes on a given IDLNode

  Args:
    node: The IDLNode to get the extended attributes from.

  Returns:
    The list of ExtAttribute IDLNodes from the node if any exist, otherwise
    returns an empty list.
  """
  ext_attribute_node = node.GetOneOf('ExtAttributes')
  if ext_attribute_node is None:
    return []
  return ext_attribute_node.GetListOf('ExtAttribute')


def HasExtendedAttribute(node: IDLNode, name: str) -> bool:
  """Returns true if the node has an extended attribute with the given name.

  Args:
    node: The IDLNode to check for the extended attribute on.
    name: The name of the extended attribute to look for.

  Returns:
    Boolean indicating if an extended attribute with the given name was found.
  """
  for extended_attribute in GetExtendedAttributes(node):
    if extended_attribute.GetName() == name:
      return True
  return False


def GetExtendedAttributeValue(node: IDLNode, name: str) -> Optional[str]:
  """Returns the string value of an extended attribute if it exists.

  Args:
    node: The IDLNode to check for the extended attribute on.
    name: The name of the extended attribute to look for.

  Returns:
    The string value of the extended attribute if found, otherwise None.
  """
  for extended_attribute in GetExtendedAttributes(node):
    if extended_attribute.GetName() == name:
      return extended_attribute.GetProperty('VALUE')
  return None


def AddCommonExtendedAttributeProperties(node: IDLNode, properties: dict):
  """Looks for common extended attributes and adds them to properties.

  Several different nodes in our IDL schemas have a common set of extended
  attributes which they all share. This helper function looks for them and adds
  the associated values to the supplied properties if they are present.

  Args:
    node: The IDLNode to look for the extended attributes on.
    properties: The object to add the associated key value pairs to.
  """
  if deprecated := GetExtendedAttributeValue(node, 'deprecated'):
    properties['deprecated'] = deprecated
  if HasExtendedAttribute(node, 'nodoc'):
    properties['nodoc'] = True
  if HasExtendedAttribute(node, 'nocompile'):
    properties['nocompile'] = True


def _ExtractNodeComment(node: IDLNode) -> str:
  """Extract contiguous file comments above a node and return them as a string.

  For comments to be extracted correctly they must be on the lines directly
  preceding the node they apply to and must use the '//' form. All contiguous
  preceding commented lines will be joined together, until a non-commented line
  is reached, with the comment characters (//) and leading/trailing whitespace
  being removed. The resulting string is returned.

  Args:
    node: The IDL node to look for a descriptive comment above.

  Returns:
    A string of all the preceding comment lines joined, ready for further
    processing.

  Raises:
    SchemaCompilerError: If top of file is reached while trying to extract a
    comment for a description.
    AssertionError: If the line number the IDLNode is annotated with is not
    greater than zero.
  """

  # Extended attributes for a node can actually be formatted onto a preceding
  # line, so if this node has an extended attribute we instead look for the
  # description relative to the extended attribute node.
  ext_attribute_node = node.GetOneOf('ExtAttributes')
  if ext_attribute_node is not None:
    return _ExtractNodeComment(ext_attribute_node)

  # Look through the lines above the current node and extract every consecutive
  # line that is a comment until a blank or non-comment line is found.
  filename, line_number = node.GetFileAndLine()

  # In theory the IDL parser shouldn't annotate any of our nodes with line
  # number 0, but in case it does we throw an error to make it obvious.
  assert line_number > 0, node.GetLogLine(
      'Attempted to extract a description comment for an IDL node, but the line'
      ' number of the node was reported as 0: %s.' % (node.GetName()))
  lines = []
  while line_number > 0:
    line = linecache.getline(filename, line_number - 1)
    # If the line starts with a double slash we treat it as a comment and add it
    # to the lines for the description.
    if line.lstrip()[:2] == '//':
      lines.insert(0, line.lstrip()[2:])
    else:
      # If we've got to a line without comment characters we've collected them
      # all and are done.
      break

    line_number -= 1
    if line_number == 1:
      # We should never reach the top of the file when trying to get a node
      # description from a file comment. If this happens, it likely means there
      # should be a blank newline.
      raise SchemaCompilerError(
          'Reached top of file when trying to parse description from file'
          ' comment. Make sure there is a blank line before the comment.',
          node,
      )
  return ''.join(lines)


class DescriptionData(NamedTuple):
  """Structured tuple to wrap documentation comment strings."""
  description: str
  parameter_descriptions: OrderedDict[str, str]


def ProcessNodeDescription(node: IDLNode) -> DescriptionData:
  """Extracts the node description and a list of any parameter descriptions.

  Extracts comments on lines directly preceding the supplied node and applies
  formatting to them. Newlines are removed, but if the comment includes
  intentional blank new lines the different "paragraphs" of the comment will be
  wrapped with a <p> tag.

  Also extracts any parameter and promise return value descriptions from the end
  of the comment and applies the above formatting to them. Parameter
  descriptions are keyed by the parameter name, followed by the description.
  Promise value descriptions are keyed using the string 'PromiseValue', then the
  name of the object the promise will resolve to, followed by the description.

  Parameter and promise value descriptions are returned as a dictionary, with
  the parameter names as keys pointing to the formatted description strings as
  values.

  For example:
    // General function documentation, can be multiple lines.
    //
    // |arg1_name|: Description of arg1.
    // |arg2_name|: Description of arg2.
    // |PromiseValue|: nameOfPromiseValue: Description of promise value.

  Will become:
  {
    description: 'General function documentation, can be multiple lines.',
    parameter_descriptions: {
      'arg1_name': 'Description of arg1.',
      'arg2_name': 'Description of arg2.',
      'PromiseValue': 'nameOfPromiseValue: Description of promise value.'
    }
  }

  TODO(crbug.com/340297705): Call this for properties.
  TODO(crbug.com/340297705): The way we handle 'PromiseValue' names/descriptions
  doesn't play well with the <p> formatting if the description for it has
  intentional blank new lines. We should fix this.

  Args:
    node: The IDL node to look for a descriptive comment above.

  Returns:
    A DescriptionData containing the formatted string for the description of the
    node and a dictionary of formatted strings for any parameter descriptions
    and PromiseValue description.
  """
  comment = _ExtractNodeComment(node)

  # Helper function to add HTML paragraphing to comments formatted with
  # intentional blank commented lines in them.
  def add_paragraphs(content):
    paragraphs = content.split('\n\n')
    if len(paragraphs) < 2:
      return content
    return '<p>' + '</p><p>'.join(p.strip() for p in paragraphs) + '</p>'

  # Helper function to strip whitespace, add paragraphing and remove newlines.
  def format_description(content):
    return add_paragraphs(content.strip()).replace('\n', '')

  # Find all the parameter comment labels of the form '|name|: comment',
  # capturing the parameter name. Note: the end of a match is actually where
  # that parameter comment starts, going until the beginning of the next match
  # (or until the end of the string).
  parameter_matches = list(re.finditer(r' *\|([^|]*)\| *: *', comment))

  # Get the parent comment (everything before the first parameter comment).
  first_parameter_location = (parameter_matches[0].start()
                              if parameter_matches else len(comment))
  description = format_description(comment[:first_parameter_location])

  # Now extract any parameter comments.
  parameter_descriptions = OrderedDict()
  # Shorthand to iterate over parameter_matches with both element N and N+1.
  for (current_param,
       next_param) in itertools.zip_longest(parameter_matches,
                                            parameter_matches[1:]):
    param_name = current_param.group(1)

    # A parameter's comment goes from the end of its introduction to the
    # beginning of the next parameter's introduction.
    param_comment_start = current_param.end()
    param_comment_end = next_param.start() if next_param else len(comment)
    parameter_descriptions[param_name] = format_description(
        comment[param_comment_start:param_comment_end])

  return DescriptionData(description, parameter_descriptions)


class Type():
  """Given an IDL node of class Type, extracts core type information.

  This class is used to extract core type information from an IDL Type node,
  creating a base dictionary object that other classes then add more properties
  to for their specific case (as things like 'name' and 'optional' differ in how
  they are determined for different kinds of typed properties).

  Attributes:
    type_node: The IDLNode for the Type to be processed.
  """

  def __init__(self,
               type_node: IDLNode,
               descriptions: Optional[OrderedDict[str, str]] = None) -> None:
    assert type_node.GetClass() in ['Type', 'Const']
    self.descriptions = descriptions
    self.type_node = type_node

  def Process(self) -> dict:
    """Processes type and returns a dict with the core information.

    For custom types this will have '$ref' key set to the name of the custom
    type. Other basic types instead use the 'type' key set to the name of the
    corresponding type the schema compiler expects to see. Promise types will
    also have a 'parameters' key for the underlying type they will resolve with.

    Returns:
      A dictionary with the core information for the type.

    Raises:
      SchemaCompilerError if the child of the IDL Type node is a class we don't
      have handling for yet.
    """

    properties = OrderedDict()
    # The Type node will have a single child, where the class and name
    # determines the underlying type it represents. This may be a fundamental
    # type or a custom type.
    # TODO(crbug.com/340297705): Add support for more types.
    type_details = self.type_node.GetChildren()[0]

    if type_details.IsA('PrimitiveType', 'StringType'):
      properties['type'] = self._TranslateBasicType(type_details)
      # 'object' types also have an 'additionalProperties' attribute and may
      # have an 'instanceOf' extended attribute.
      if properties['type'] == 'object':
        properties['additionalProperties'] = {'type': 'any'}
        if instance_of := GetExtendedAttributeValue(self.type_node.GetParent(),
                                                    'instanceOf'):
          properties['isInstanceOf'] = instance_of
    elif type_details.IsA('Typeref'):
      # Some common types don't actually have a custom class backing them and
      # are just Typerefs with a string name.
      if type_details.GetName() == 'ArrayBuffer':
        properties['type'] = 'binary'
        properties['isInstanceOf'] = 'ArrayBuffer'
      else:
        # Other Typerefs will either be referencing a custom type defined as a
        # Dictionary/Enum or a function defined as a Callback in the schema
        # file. For custom types we just add a '$ref' with the type name,
        # but functions we embed similar to how we normally process Operations.
        type_name = type_details.GetName()
        # Custom types and Callback functions are defined at the top level of
        # the IDL file, so we need to recurse up the tree to the File node to
        # look for them.
        parent = self.type_node
        while parent.GetClass() != 'File':
          parent = parent.GetParent()

        referenced_type = GetChildWithName(parent, type_name)
        # TODO(crbug.com/450443604): Add support for shared types, which are
        # defined in a separate file that is referenced by several different
        # schemas.
        if referenced_type is None:
          raise SchemaCompilerError(
              'Could not find definition of referenced type "%s" for node.' %
              type_name,
              type_details,
          )

        if referenced_type.GetClass() in ['Dictionary', 'Enum']:
          properties['$ref'] = type_name
        elif referenced_type.GetClass() == 'Callback':
          properties = Operation(referenced_type).process()
        else:
          raise SchemaCompilerError(
              'Found a Typeref node referencing a node of type "%s", but we'
              ' only support Typerefs that reference Dictionary, Enum or'
              ' Callback class nodes.' % referenced_type.GetClass(),
              type_details,
          )

    elif type_details.IsA('Undefined'):
      properties['type'] = UndefinedType
    elif type_details.IsA('Promise'):
      # Promise types have an associated type they resolve with. We represent
      # this similar to how we represent arguments for Operations, with a
      # 'parameters' list that has a single element for the type.
      properties['parameters'] = self._ExtractParametersFromPromiseType(
          type_details, self.descriptions)
      # TODO(crbug.com/428187556): It would be nice to explicitly mark these as
      # 'type' = 'promise' as well once we're done migrating schemas to WebIDL.
    elif type_details.IsA('Sequence'):
      properties['type'] = 'array'
      # Sequences are used to represent array types, which have an associated
      # 'items' key that detail what type the array holds.
      properties['items'] = ArrayType(type_details).Process()
    elif type_details.IsA('Any'):
      properties['type'] = 'any'
    elif type_details.IsA('UnionType'):
      properties['choices'] = [
          Type(node).Process() for node in type_details.GetListOf('Type')
      ]
    else:
      raise SchemaCompilerError('Unsupported type class when processing type.',
                                type_details)
    return properties

  def _TranslateBasicType(self, type_details: IDLNode) -> str:
    """Translates a basic IDL type into the corresponding python type.

    Handles both PrimitiveType and StringType class nodes, as their handling is
    the same.

    Returns:
      A string representing the name of the equivalent python type the schema
      compiler uses.

    Raises:
      SchemaCompilerError if the PrimitiveType 'void' was used as it is
      deprecated, or if we encountered a basic type we don't have handling for
      yet.
    """

    type_name = type_details.GetName()
    if type_name == 'void':
      raise SchemaCompilerError(
          'Usage of "void" in IDL is deprecated, use "Undefined" instead.',
          type_details)
    if type_name == 'boolean':
      return 'boolean'
    if type_name == 'double':
      return 'number'
    if type_name == 'long':
      return 'integer'
    if type_name == 'DOMString':
      return 'string'
    if type_name == 'object':
      return 'object'

    raise SchemaCompilerError(
        'Unsupported basic type found when processing type.', type_details)

  def _ExtractParametersFromPromiseType(
      self,
      type_details: IDLNode,
      descriptions: Optional[OrderedDict[str, str]] = None) -> List[dict]:
    """Extracts details for the type a promise will resolve to.

    Returns:
      A list with a single dictionary that represents the details of the type
      the promise will resolve to. Note: Even though this can only be a single
      element, this uses a list to mirror the 'parameters' key used on function
      definitions.
    """

    promise_type = PromiseType(type_details, descriptions).Process()
    if 'type' in promise_type and promise_type['type'] is UndefinedType:
      # If the promise type was 'Undefined' we represent it as an empty list.
      return []
    return [promise_type]


class TypedProperty(ABC):
  """Abstract base class for properties that have type information.

  This base class is responsible for extracting the base type information that
  is common to several different kinds of properties. Subclasses then override
  the Process method to add other properties such as the name and description.

  Attributes:
    node: The IDLNode that represents this property.
    descriptions: A dictionary of comment description strings, used for passing
      in descriptions of function arguments.
    type_node: The specific IDLNode of class Type which contains type details.
    properties: The dictionary for the final processed representation of this
      typed property which will be returned when calling Process.
  """

  def __init__(self,
               node: IDLNode,
               descriptions: Optional[OrderedDict[str, str]] = None) -> None:
    self.node = node
    self.descriptions = descriptions
    self.type_node = node.GetOneOf('Type')
    assert self.type_node is not None, self.type_node.GetLogLine(
        'Could not find Type node on IDLNode named: %s.' % (node.GetName()))
    self.properties = Type(self.type_node, descriptions).Process()

  @abstractmethod
  def Process(self) -> dict:
    """Processes the property and returns a dict representing it."""


class FunctionArgument(TypedProperty):
  """Handles processing for function arguments."""

  def Process(self) -> dict:
    name = self.node.GetName()
    self.properties['name'] = name
    if self.descriptions and name in self.descriptions:
      self.properties['description'] = self.descriptions[name]
    if self.node.GetProperty('OPTIONAL'):
      self.properties['optional'] = True
    return self.properties


class FunctionReturn(TypedProperty):
  """Handles processing for function return values."""

  def Process(self) -> dict:
    # If the descriptions use the 'Returns' key, we use that to extract a
    # description to add to the return properties.
    if self.descriptions and 'Returns' in self.descriptions:
      self.properties['description'] = self.descriptions['Returns']
    # If no type was specified but there is a parameters property, we can infer
    # this is a promise definition for an asynchronous return.
    if 'type' not in self.properties and 'parameters' in self.properties:
      # For legacy reasons, promise returns always get named "callback".
      self.properties['name'] = 'callback'
    else:
      self.properties['name'] = self.node.GetName()
    return self.properties


class PromiseType(TypedProperty):
  """Handles processing for the type a promise will resolve with."""

  def Process(self) -> dict:
    if self.type_node.GetProperty('NULLABLE'):
      self.properties['optional'] = True
    # If the descriptions use the 'PromiseValue' key, we use that to extract the
    # name and any description for the typed value the promise will resolve to.
    # The comment consists of the name to use, followed by an optional
    # description string indicated by a colon + space and then the description.
    if self.descriptions and 'PromiseValue' in self.descriptions:
      name_and_description = self.descriptions['PromiseValue'].split(': ', 1)
      self.properties['name'] = name_and_description.pop(0)
      # We only add the promise value description if one was included in the
      # comment after the name.
      if name_and_description:
        self.properties['description'] = name_and_description.pop()
    return self.properties


class ArrayType(TypedProperty):
  """Handles processing for the type an array (IDL Sequence) consists of."""

  def Process(self) -> dict:
    return self.properties


class DictionaryMember(TypedProperty):
  """Handles processing for members of custom types (dictionaries)."""

  def Process(self) -> dict:
    # TODO(crbug.com/340297705): Add support for extended attributes on custom
    # type members.
    self.properties['name'] = self.node.GetName()

    if not self.node.GetProperty('REQUIRED'):
      self.properties['optional'] = True

    AddCommonExtendedAttributeProperties(self.node, self.properties)

    description = ProcessNodeDescription(self.node).description
    if description:
      self.properties['description'] = description
    return self.properties


class Operation:
  """Represents an API function and processes the details of it.

  Given an IDLNode representing an API function, processes it into a Python
  dictionary that the JSON schema compiler expects to see.

  Attributes:
    node: The IDLNode for the Operation definition that represents this
      function.
  """

  def __init__(self, node: IDLNode) -> None:
    self.node = node

  def process(self) -> dict:
    properties = {}
    properties['name'] = self.node.GetName()
    properties['type'] = 'function'

    description_data = ProcessNodeDescription(self.node)
    if (description_data.description):
      properties['description'] = description_data.description

    AddCommonExtendedAttributeProperties(self.node, properties)
    if platforms := GetExtendedAttributeValue(self.node, 'platforms'):
      properties['platforms'] = platforms

    parameters = []
    arguments_node = self.node.GetOneOf('Arguments')
    for argument in arguments_node.GetListOf('Argument'):
      parameters.append(
          FunctionArgument(argument,
                           description_data.parameter_descriptions).Process())
    properties['parameters'] = parameters

    # Return type processing.
    return_type = FunctionReturn(
        self.node, description_data.parameter_descriptions).Process()
    if 'type' in return_type and return_type['type'] is UndefinedType:
      # This is an Undefined return, so we don't add anything.
      pass
    # If no type was specified but there is a parameters property, we can infer
    # this is a promise definition for an asynchronous return.
    elif 'type' not in return_type and 'parameters' in return_type:
      # TODO(tjudkins): The optionality of the callback is only relevant for
      # contexts that don't support promise based calls and for the few
      # functions which don't support promise based calls, as the callback is
      # always inherently optional when using a promise based call instead. It
      # would be nice to just get rid of the 'optional' property here and always
      # treat it as optional when we remove the context restrictions for promise
      # based calls.
      if not HasExtendedAttribute(self.node, 'requiredCallback'):
        return_type['optional'] = True
      # For legacy reasons Promise based returns are represented on a
      # "returns_async" property.
      # TODO(crbug.com/428187556): Once we've migrated schemas to WebIDL, we
      # should be able to just use the 'returns' field with 'type' = 'promise'
      # instead of the 'returns_async' property.
      properties['returns_async'] = return_type
    else:
      # Otherwise this is a typed return using either the 'type' key or '$ref'
      # key to reference the underlying type.
      properties['returns'] = return_type

    return properties


class Dictionary:
  """Represents an API type and processes the details of it.

  Given an IDLNode of class Dictionary, converts it into a Python dictionary
  representing a custom "type" for the API.

  Attributes:
    node: The IDLNode for the Dictionary definition that represents this type.
  """

  def __init__(self, node: IDLNode) -> None:
    self.node = node

  def process(self) -> dict:
    properties = OrderedDict()
    for property_node in self.node.GetListOf('Key'):
      properties[property_node.GetName()] = DictionaryMember(
          property_node).Process()

    result = {
        'id': self.node.GetName(),
        'properties': properties,
        'type': 'object'
    }
    AddCommonExtendedAttributeProperties(self.node, result)

    return result


class Enum:
  """Represents an API enum and processes the details of it.

  Given an IDLNode of class Enum, converts it into a Python dictionary
  representing an enumeration for the API.

  Attributes:
    node: The IDLNode for the Enum definition that represents this type.
  """

  def __init__(self, node: IDLNode) -> None:
    self.node = node

  def process(self) -> dict:
    enum = []
    for enum_item in self.node.GetListOf('EnumItem'):
      enum_value = {'name': enum_item.GetName()}
      value_description = ProcessNodeDescription(enum_item).description
      if value_description:
        enum_value['description'] = value_description
      enum.append(enum_value)
    result = {
        'id': self.node.GetName(),
        'description': ProcessNodeDescription(self.node).description,
        'type': 'string',
        'enum': enum
    }
    AddCommonExtendedAttributeProperties(self.node, result)

    return result


class Event:
  """Represents an API event and processes the details of it.

  Given an IDLNode of class Attribute for an event, extracts out the details of
  the associated event callback and converts it to a Python dictionary
  representing it.

  Attributes:
    node: The IDLNode for the Attribute definition for this event.
  """

  def __init__(self, node: IDLNode) -> None:
    self.node = node

  def process(self, parent: IDLNode) -> dict:
    # Double check that the parent passed in is the top level 'File' class node.
    assert parent.GetClass() == 'File'

    properties = OrderedDict()
    properties['name'] = self.node.GetName()

    # Events just store the details of the event callback function, hence the
    # type is considered 'function'.
    properties['type'] = 'function'

    # Getting at the arguments for the event listener Callback definition
    # requires some bouncing around the parsed IDL. The Attribute exposing the
    # event has a Typeref which should be defined as an Interface on the top
    # level of the IDL file. This Interface in turn lists the functions for
    # adding/removing listeners. To find the listener arguments, we look for the
    # 'addListener' Operation and then look for the Typeref defined in the
    # Arguments for it which will be a Callback, which we can then look for
    # defined on the top level of the IDL file.
    interface_name = GetTypeName(self.node)
    event_interface = GetChildWithName(parent, interface_name)
    if event_interface is None or event_interface.GetClass() != 'Interface':
      raise SchemaCompilerError(
          'Could not find Interface definition for event.', self.node)
    self._VerifyEventDefinition(event_interface)
    add_listener_operation = GetChildWithName(event_interface, 'addListener')
    callback_name = GetTypeName(
        add_listener_operation.GetOneOf('Arguments').GetOneOf('Argument'))
    callback_node = GetChildWithName(parent, callback_name)
    parameter_descriptions = ProcessNodeDescription(
        callback_node).parameter_descriptions

    description = ProcessNodeDescription(self.node).description
    if (description):
      properties['description'] = description

    parameters = []
    arguments_node = callback_node.GetOneOf('Arguments')
    for argument in arguments_node.GetListOf('Argument'):
      parameters.append(
          FunctionArgument(argument, parameter_descriptions).Process())
    properties['parameters'] = parameters

    AddCommonExtendedAttributeProperties(self.node, properties)

    return properties

  def _VerifyEventDefinition(self, event: IDLNode) -> None:
    """Verifies the event has the expected Operations and inheritance.

    Used to verify that an event definition in the IDL file has all the required
    Operation definitions on it and inherits from ExtensionEvent, raising an
    exception if anything is wrong. Intended primarily to catch mistakes in IDL
    API definitions.

    Args:
      event: The IDLNode for the event Interface to validate.

    Raises:
      SchemaCompilerError if any of the required definitions are not present.
    """

    inherit_node = GetChildWithName(event, 'ExtensionEvent')
    if inherit_node is None or inherit_node.GetClass() != 'Inherit':
      raise SchemaCompilerError(
          'Event Interface missing ExtensionEvent Inheritance.', event)

    add_listener = GetChildWithName(event, 'addListener')
    if add_listener is None or add_listener.GetClass() != 'Operation':
      raise SchemaCompilerError(
          'Event Interface missing addListener Operation definition.', event)
    remove_listener = GetChildWithName(event, 'removeListener')
    if remove_listener is None or remove_listener.GetClass() != 'Operation':
      raise SchemaCompilerError(
          'Event Interface missing removeListener Operation definition.', event)
    has_listener = GetChildWithName(event, 'hasListener')
    if has_listener is None or has_listener.GetClass() != 'Operation':
      raise SchemaCompilerError(
          'Event Interface missing hasListener Operation definition.', event)


class Property:
  """Represents a property on an API namespace and processes the details of it.

  Given an IDLNode of type Const, processes it into the key value pair for it to
  be exposed as a property on an API namespace.

  Attributes:
    node: The IDLNode for the Const definition that represents this type.
  """

  def __init__(self, node: IDLNode) -> None:
    self.node = node

  def process(self) -> (str, dict):
    properties = Type(self.node).Process()
    value = self.node.GetOneOf('Value').GetProperty('VALUE')
    # Unfortunately, WebIDL doesn't allow string values for consts, so we have
    # to hack them in using an extended attribute.
    if properties['type'] == 'string':
      value = GetExtendedAttributeValue(self.node, 'StringValue')
      if value is None:
        raise SchemaCompilerError(
            'If using a const of type DOMString, you must specify the extended'
            ' attribute "StringValue" for the value.',
            self.node,
        )
    # The IDL Parser always returns values as strings, so cast to their real
    # type.
    properties['value'] = self._CastFromType(properties['type'], value)

    description_data = ProcessNodeDescription(self.node)
    if (description_data.description):
      properties['description'] = description_data.description

    AddCommonExtendedAttributeProperties(self.node, properties)

    return (self.node.GetName(), properties)

  def _CastFromType(self, type_name: str,
                    string_value: str) -> Union[int, float, str]:
    """Casts from a string value to a real Python type based on type name.

    Args:
      type_name: The string representing the name of the Schema Compiler type to
      cast using.
      string_value: The string representation of the value to try and cast.

    Returns:
      The value cast to the appropriate Python type
    """
    if type_name == 'integer':
      return int(string_value)
    if type_name == 'number':
      return float(string_value)
    return string_value


class Namespace:
  """Represents an API namespace and processes individual details of it.

  Given an IDLNode that is the root of a tree representing an API Interface,
  processes it into a Python dictionary that the JSON schema compiler expects to
  see.

  Attributes:
    name: The name the API namespace will be exposed on.
    namespace_node: The root IDLNode for the abstract syntax tree representing
      this namespace.
  """

  def __init__(self, name: str, namespace_node: IDLNode) -> None:
    """Initializes the instance with the namespace name and root IDLNode.

    Args:
      name: The name the API namespace will be exposed on.
      namespace_node: The root IDLNode for the abstract syntax tree representing
        this namespace.
    """
    self.name = name
    self.namespace = namespace_node

  def process(self) -> dict:
    functions = []
    types = []
    events = []
    properties = OrderedDict()
    manifest_keys = None
    description = ProcessNodeDescription(self.namespace).description

    # Functions are defined as Operations on the API Interface definition.
    for node in self.namespace.GetListOf('Operation'):
      functions.append(Operation(node).process())

    # Enums and Dictionary defined custom types are included at the top level of
    # the IDL file, on the parent node of the API interface definitions. To
    # retain the ordering from the schema, we loop over this full set of nodes
    # one by one.
    for node in self.namespace.GetParent().GetChildren():
      if node.GetClass() == 'Enum':
        types.append(Enum(node).process())
      if node.GetClass() == 'Dictionary':
        # Manifest keys defined in the schema are separate from normal custom
        # types and instead get put into the manifest_keys property.
        if node.GetName() == 'Manifest':
          if not node.GetProperty('PARTIAL'):
            raise SchemaCompilerError(
                'If using a "Manifest" dictionary to define manifest keys, it'
                ' must be declared "partial".',
                node,
            )
          manifest_keys = Dictionary(node).process()['properties']
          continue
        # Otherwise this is a normal Dictionary defined type and goes in the
        # normal types.
        types.append(Dictionary(node).process())

    # Events are defined as Attributes on the API Interface definition, which
    # use types that are defined as Interfaces on the top level of the IDL file.
    for node in self.namespace.GetListOf('Attribute'):
      events.append(Event(node).process(self.namespace.GetParent()))

    # Properties are defined with Consts on the API Interface definition.
    for node in self.namespace.GetListOf('Const'):
      property_key, property_value = Property(node).process()
      properties[property_key] = property_value

    result = {
        'namespace': self.name,
        'functions': functions,
        'types': types,
        'events': events,
        'properties': properties,
        'manifest_keys': manifest_keys,
        'description': description,
        'nodoc': False,
        'platforms': None,
        'deprecated': None,
        'compiler_options': {},
    }

    # Several special attributes specific to the schema compilation process are
    # defined using Extended Attributes on the API Interface definition.
    AddCommonExtendedAttributeProperties(self.namespace, result)
    if platforms := GetExtendedAttributeValue(self.namespace, 'platforms'):
      result['platforms'] = platforms
    if implemented_in := GetExtendedAttributeValue(self.namespace,
                                                   'implemented_in'):
      result['compiler_options']['implemented_in'] = implemented_in
    if HasExtendedAttribute(self.namespace, 'generate_error_messages'):
      result['compiler_options']['generate_error_messages'] = True

    return result


class IDLSchema:
  """Holds the entirety of a parsed IDL schema, ready to process further.

  Given an abstract syntax tree of IDLNodes and IDLAttributes, converts into a
  Python list of API namespaces that the JSON schema compiler expects to see.

  Attributes:
    idl: The parsed tree of IDL Nodes from the IDL parser.
  """

  def __init__(self, idl: IDLNode) -> None:
    """Initializes the instance with the parsed tree of IDL nodes.

    Args:
      idl: The parsed tree of IDL Nodes from the IDL parser.
    """
    self.idl = idl

  def process(self) -> dict:
    namespaces = []
    # TODO(crbug.com/340297705): Eventually this will need be changed to support
    # processing "shared types", which are not exposed on a Browser interface.
    browser_node = GetChildWithName(self.idl, 'Browser')
    if browser_node is None or browser_node.GetClass() != 'Interface':
      raise SchemaCompilerError(
          'Required partial Browser interface not found in schema.', self.idl)

    # The 'Browser' Interface has one attribute describing the name this API is
    # exposed on.
    attributes = browser_node.GetListOf('Attribute')
    if len(attributes) != 1:
      raise SchemaCompilerError(
          'The partial Browser interface should have exactly one attribute for'
          ' the name the API will be exposed under.',
          browser_node,
      )
    api_name = attributes[0].GetName()
    idl_type = GetTypeName(attributes[0])

    namespace_node = GetChildWithName(self.idl, idl_type)

    # If the API interface is a partial interface, it means it's part of a
    # nested interface (an API name with a dot in it) and we need to go another
    # layer deeper.
    while namespace_node.GetProperty('PARTIAL'):
      attributes = namespace_node.GetListOf('Attribute')
      api_name += '.' + attributes[0].GetName()
      idl_type = GetTypeName(attributes[0])

      namespace_node = GetChildWithName(self.idl, idl_type)

    namespace = Namespace(
        api_name,
        namespace_node,
    )
    namespaces.append(namespace.process())

    return namespaces


def Load(filename):
  """Loads and processes an IDL file into a dictionary.

  Given the filename of an IDL file, parses it and returns an equivalent Python
  dictionary in a format that the JSON schema compiler expects to see.

  Args:
    filename: A string of the filename of the IDL file to be parsed.

  Returns:
    A dictionary representing the parsed API schema details.
  """

  parser = idl_parser.IDLParser(idl_lexer.IDLLexer())
  idl = idl_parser.ParseFile(parser, filename)
  idl_schema = IDLSchema(idl)
  return idl_schema.process()


def Main():
  """Dumps the result of loading and processing IDL files to command line.

  Dump a json serialization of parse results for the IDL files whose names were
  passed in on the command line or whose contents is piped in. Mostly used for
  manual testing, consumers will generally call Load directly instead.
  """
  if len(sys.argv) > 1:
    for filename in sys.argv[1:]:
      schema = Load(filename)
      print(json.dumps(schema, indent=2))
  else:
    contents = sys.stdin.read()
    for i, char in enumerate(contents):
      if not char.isascii():
        raise Exception(
            'Non-ascii character "%s" (ord %d) found at offset %d.' %
            (char, ord(char), i))
    idl = idl_parser.IDLParser().ParseData(contents, '<stdin>')
    schema = IDLSchema(idl).process()
    print(json.dumps(schema, indent=2))


if __name__ == '__main__':
  Main()
