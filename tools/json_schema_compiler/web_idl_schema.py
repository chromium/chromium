#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import json
import linecache
import os.path
import re
import sys
from abc import ABC, abstractmethod
from typing import Dict, List, Optional, NamedTuple
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
if _idl_generators_path in sys.path:
  from idl_parser import idl_parser, idl_lexer, idl_node
else:
  sys.path.insert(0, _idl_generators_path)
  try:
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

  # The IDL parser doesn't annotate Operation nodes with their line number
  # correctly, but the Arguments child node will have the correct line number,
  # so use that instead.
  if node.GetClass() == 'Operation':
    return _ExtractNodeComment(node.GetOneOf('Arguments'))

  # Extended attributes for a node can actually be formatted onto a preceding
  # line, so if this node has an extended attribute we instead look for the
  # description relative to the extended attribute node.
  ext_attribute_node = node.GetOneOf('ExtAttributes')
  if ext_attribute_node is not None:
    return _ExtractNodeComment(ext_attribute_node)

  # Look through the lines above the current node and extract every consecutive
  # line that is a comment until a blank or non-comment line is found.
  filename, line_number = node.GetFileAndLine()
  # The IDL parser we use doesn't annotate some classes of nodes with the
  # correct line number and just reports them as line 0. In theory we shouldn't
  # pass any of those nodes to this function, so throw an error if happens.
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

  Uses _ExtractNodeComment to first get the comments on lines directly preceding
  the supplied node and then applies formatting to them. Newlines are removed,
  but if the comment also includes intentional blank new lines, the different
  "paragraphs" of the comment will be wrapped with a <p> tag.

  Also extracts out parameter descriptions and applies the above formatting to
  them. Parameter comments must come at the end of the comment and be of the
  form:
    // General function documentation, can be multiple lines.
    //
    // |arg1_name|: Description of arg1.
    // |arg2_name|: Description of arg2...
  Parameter descriptions are returned as a dictionary, with the parameter names
  as keys pointing to the formatted description strings as values.

  TODO(crbug.com/340297705): Call this for properties.

  Args:
    node: The IDL node to look for a descriptive comment above.

  Returns:
    A DescriptionData containing the formatted string for the description of the
    node and a dictionary of formatted strings for any parameter descriptions.
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

  def __init__(self, type_node: IDLNode) -> None:
    assert type_node.GetClass() == 'Type'
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
    elif type_details.IsA('Typeref'):
      # For custom types the name indicates the underlying referenced type.
      # TODO(crbug.com/340297705): We should verify this ref name is actually a
      # custom type we have parsed from the IDL.
      properties['$ref'] = type_details.GetName()
    elif type_details.IsA('Undefined'):
      properties['type'] = UndefinedType
    elif type_details.IsA('Promise'):
      properties['type'] = 'promise'
      # Promise types also have an associated type they resolve with. We
      # represent this similar to how we represent arguments for Operations,
      # with 'parameters' list that has a single element for the type.
      properties['parameters'] = self._ExtractParametersFromPromiseType(
          type_details)
    elif type_details.IsA('Sequence'):
      properties['type'] = 'array'
      # Sequences are used to represent array types, which have an associated
      # 'items' key that detail what type the array holds.
      properties['items'] = ArrayType(type_details).Process()
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

    raise SchemaCompilerError(
        'Unsupported basic type found when processing type.', type_details)

  def _ExtractParametersFromPromiseType(self,
                                        type_details: IDLNode) -> List[dict]:
    """Extracts details for the type a promise will resolve to.

    Returns:
      A list with a single dictionary that represents the details of the type
      the promise will resolve to. Note: Even though this can only be a single
      element, this uses a list to mirror the 'parameters' key used on function
      definitions.
    """

    promise_type = PromiseType(type_details).Process()
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
    self.properties = Type(self.type_node).Process()

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
    if 'type' in self.properties and self.properties['type'] == 'promise':
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
    # We consider nullable properties on custom types as being "optional" in the
    # schema compiler's logic.
    if self.type_node.GetProperty('NULLABLE'):
      self.properties['optional'] = True

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
    properties = OrderedDict()
    properties['name'] = self.node.GetName()

    description_data = ProcessNodeDescription(self.node)
    if (description_data.description):
      properties['description'] = description_data.description

    parameters = []
    arguments_node = self.node.GetOneOf('Arguments')
    for argument in arguments_node.GetListOf('Argument'):
      parameters.append(
          FunctionArgument(argument,
                           description_data.parameter_descriptions).Process())
    properties['parameters'] = parameters

    # Return type processing.
    return_type = FunctionReturn(self.node).Process()
    if 'type' in return_type and return_type['type'] is UndefinedType:
      # This is an Undefined return, so we don't add anything.
      pass
    elif 'type' in return_type and return_type['type'] == 'promise':
      # For legacy reasons Promise based returns are represented on a
      # "returns_async" property.
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

    # The WebIDL Parser incorrectly reports the line number for Attributes we
    # use to define events as 0, so we need to use the Typeref node on the
    # Attribute instead to get the correct line number to extract the
    # description comment.
    # TODO(crbug.com/396176041): Clean this up once the line number issue is
    # resolved in the Parser.
    description = ProcessNodeDescription(
        self.node.GetOneOf('Type').GetOneOf('Typeref')).description
    if (description):
      properties['description'] = description

    parameters = []
    arguments_node = callback_node.GetOneOf('Arguments')
    for argument in arguments_node.GetListOf('Argument'):
      parameters.append(
          FunctionArgument(argument, parameter_descriptions).Process())
    properties['parameters'] = parameters

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
    nodoc = False
    platforms = None
    compiler_options = OrderedDict()
    deprecated = None

    # Functions are defined as Operations on the API Interface definition.
    for node in self.namespace.GetListOf('Operation'):
      functions.append(Operation(node).process())

    # Types are defined as Dictionaries at the top level of the IDL file, which
    # are found on the parent node of the API Interface definition.
    for node in self.namespace.GetParent().GetListOf('Dictionary'):
      types.append(Dictionary(node).process())

    # Events are defined as Attributes on the API Interface definition, which
    # use types that are defined as Interfaces on the top level of the IDL file.
    for node in self.namespace.GetListOf('Attribute'):
      events.append(Event(node).process(self.namespace.GetParent()))

    for extended_attribute in GetExtendedAttributes(self.namespace):
      attribute_name = extended_attribute.GetName()
      if attribute_name == 'nodoc':
        nodoc = True
      elif attribute_name == 'platforms':
        platforms = extended_attribute.GetProperty('VALUE')
      else:
        raise SchemaCompilerError(
            f'Unknown extended attribute with name "{attribute_name}" when'
            ' processing namespace.', self.namespace)

    return {
        'namespace': self.name,
        'functions': functions,
        'types': types,
        'events': events,
        'properties': properties,
        'manifest_keys': manifest_keys,
        'nodoc': nodoc,
        'description': description,
        'platforms': platforms,
        'compiler_options': compiler_options,
        'deprecated': deprecated,
    }


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
