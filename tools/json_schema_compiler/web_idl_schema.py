#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os.path
import sys
from typing import List, Optional
from json_parse import OrderedDict

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


class Type:
  """Represents an IDL type and maps it to the corresponding python type.

  Given a Type node representing the type of a dictionary member, function
  parameter or return, converts it into a Python dictionary the JSON schema
  compiler expects to see.

  Attributes:
    node: The IDLNode that represents this type.
    additional_properties: A dictionary of additional key value pairs to be
      included on the resulting dictionary after processing.
  """

  def __init__(self, node: IDLNode, additional_properties: dict) -> None:
    assert node.GetClass() == 'Type', node.GetLogLine(
        'Attempted to process a "Type" node, but was passed a "%s" node.' %
        (node.GetClass()))
    self.node = node
    self.additional_properties = additional_properties

  def process(self) -> dict:
    properties = self.additional_properties
    basic_type = self.node.GetOneOf('PrimitiveType', 'StringType')
    if basic_type:
      name = basic_type.GetName()
      if name == 'void':
        # If it's a void return, we bail early.
        return None

      if name == 'boolean':
        properties['type'] = 'boolean'
      elif name == 'double':
        properties['type'] = 'number'
      elif name == 'long':
        properties['type'] = 'integer'
      elif name == 'DOMString':
        properties['type'] = 'string'
      else:
        raise SchemaCompilerError(
            'Unsupported basic type found when processing type.', basic_type)
    else:
      unknown_child = self.node.GetChildren()[0]
      raise SchemaCompilerError('Unsupported type class when processing type.',
                                unknown_child)

    return properties


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

    # Return type processing.
    type_node = self.node.GetOneOf('Type')
    return_type = Type(type_node, {'name': self.node.GetName()}).process()
    if return_type is not None:
      properties['returns'] = return_type

    return properties


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

    for node in self.namespace.GetListOf('Operation'):
      functions.append(Operation(node).process())

    nodoc = 'nodoc' in [
        attribute.GetName()
        for attribute in GetExtendedAttributes(self.namespace)
    ]

    return {
        'namespace': self.name,
        'functions': functions,
        'nodoc': nodoc,
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
    namespace = Namespace(api_name, namespace_node)
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
