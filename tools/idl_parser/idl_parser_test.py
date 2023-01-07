#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import unittest

from idl_lexer import IDLLexer
from idl_parser import IDLParser, ParseFile


def ParseCommentTest(comment):
  comment = comment.strip()
  comments = comment.split(None, 1)
  return comments[0], comments[1]


class WebIDLParser(unittest.TestCase):

  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)
    test_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), 'test_parser'))
    self.filenames = glob.glob('%s/*_web.idl' % test_dir)

  def _TestNode(self, node, filepath):
    comments = node.GetListOf('SpecialComment')
    for comment in comments:
      check, value = ParseCommentTest(comment.GetName())
      if check == 'ERROR':
        msg = node.GetLogLine('Expecting\n\t%s\nbut found \n\t%s\n' % (
            value, str(node)))
        self.assertEqual(value, node.GetName() ,msg)

      if check == 'TREE':
        quick = '\n'.join(node.Tree())
        lineno = node.GetProperty('LINENO')
        msg = 'Mismatched tree at line %d in %s:' % (lineno, filepath)
        msg += '\n\n[EXPECTED]\n%s\n\n[ACTUAL]\n%s\n' % (value, quick)
        self.assertEqual(value, quick, msg)

  def testExpectedNodes(self):
    for filename in self.filenames:
      filenode = ParseFile(self.parser, filename)
      children = filenode.GetChildren()
      self.assertTrue(len(children) > 2, 'Expecting children in %s.' %
          filename)

      for node in filenode.GetChildren():
        self._TestNode(node, filename)


class TestIncludes(unittest.TestCase):

  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)

  def _ParseIncludes(self, idl_text):
    filenode = self.parser.ParseText(filename='', data=idl_text)
    self.assertEqual(1, len(filenode.GetChildren()))
    return filenode.GetChildren()[0]

  def testAIncludesB(self):
    idl_text = 'A includes B;'
    includes_node = self._ParseIncludes(idl_text)
    self.assertEqual('Includes(A)', str(includes_node))
    reference_node = includes_node.GetProperty('REFERENCE')
    self.assertEqual('B', str(reference_node))

  def testBIncludesC(self):
    idl_text = 'B includes C;'
    includes_node = self._ParseIncludes(idl_text)
    self.assertEqual('Includes(B)', str(includes_node))
    reference_node = includes_node.GetProperty('REFERENCE')
    self.assertEqual('C', str(reference_node))

  def testUnexpectedSemicolon(self):
    idl_text = 'A includes;'
    node = self._ParseIncludes(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected ";" after keyword "includes".',
        error_message)

  def testUnexpectedIncludes(self):
    idl_text = 'includes C;'
    node = self._ParseIncludes(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected includes.',
        error_message)

  def testUnexpectedIncludesAfterBracket(self):
    idl_text = '[foo] includes B;'
    node = self._ParseIncludes(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected keyword "includes" after "]".',
        error_message)


class TestEnums(unittest.TestCase):

  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)

  def _ParseEnums(self, idl_text):
    filenode = self.parser.ParseText(filename='', data=idl_text)
    self.assertEqual(1, len(filenode.GetChildren()))
    return filenode.GetChildren()[0]

  def testBasic(self):
    idl_text = 'enum MealType {"rice","noodles","other"};'
    node = self._ParseEnums(idl_text)
    children = node.GetChildren()
    self.assertEqual('Enum', node.GetClass())
    self.assertEqual(3, len(children))
    self.assertEqual('EnumItem', children[0].GetClass())
    self.assertEqual('rice', children[0].GetName())
    self.assertEqual('EnumItem', children[1].GetClass())
    self.assertEqual('noodles', children[1].GetName())
    self.assertEqual('EnumItem', children[2].GetClass())
    self.assertEqual('other', children[2].GetName())

  def testErrorMissingName(self):
    idl_text = 'enum {"rice","noodles","other"};'
    node = self._ParseEnums(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Enum missing name.', error_message)

  def testTrailingCommaIsAllowed(self):
    idl_text = 'enum TrailingComma {"rice","noodles","other",};'
    node = self._ParseEnums(idl_text)
    children = node.GetChildren()
    self.assertEqual('Enum', node.GetClass())
    self.assertEqual(3, len(children))
    self.assertEqual('EnumItem', children[0].GetClass())
    self.assertEqual('rice', children[0].GetName())
    self.assertEqual('EnumItem', children[1].GetClass())
    self.assertEqual('noodles', children[1].GetName())
    self.assertEqual('EnumItem', children[2].GetClass())
    self.assertEqual('other', children[2].GetName())

  def testErrorMissingCommaBetweenIdentifiers(self):
    idl_text = 'enum MissingComma {"rice" "noodles","other"};'
    node = self._ParseEnums(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected string "noodles" after string "rice".',
        error_message)

  def testErrorExtraCommaBetweenIdentifiers(self):
    idl_text = 'enum ExtraComma {"rice","noodles",,"other"};'
    node = self._ParseEnums(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected "," after ",".', error_message)

  def testErrorUnexpectedKeyword(self):
    idl_text = 'enum TestEnum {interface,"noodles","other"};'
    node = self._ParseEnums(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected keyword "interface" after "{".',
        error_message)

  def testErrorUnexpectedIdentifier(self):
    idl_text = 'enum TestEnum {somename,"noodles","other"};'
    node = self._ParseEnums(idl_text)
    self.assertEqual('Error', node.GetClass())
    error_message = node.GetName()
    self.assertEqual('Unexpected identifier "somename" after "{".',
        error_message)

class TestExtendedAttribute(unittest.TestCase):
  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)

  def _ParseIdlWithExtendedAttributes(self, extended_attribute_text):
    idl_text = extended_attribute_text + ' enum MealType {"rice"};'
    filenode = self.parser.ParseText(filename='', data=idl_text)
    self.assertEqual(1, len(filenode.GetChildren()))
    node = filenode.GetChildren()[0]
    self.assertEqual('Enum', node.GetClass())
    children = node.GetChildren()
    self.assertEqual(2, len(children))
    self.assertEqual('EnumItem', children[0].GetClass())
    self.assertEqual('rice', children[0].GetName())
    return children[1]

  def testNoArguments(self):
    extended_attribute_text = '[Replacable]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()) )
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('Replacable', attribute.GetName())

  def testArgumentList(self):
    extended_attribute_text = '[Constructor(double x, double y)]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()))
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('Constructor', attribute.GetName())
    self.assertEqual('Arguments', attribute.GetChildren()[0].GetClass())
    arguments = attributes.GetChildren()[0].GetChildren()[0]
    self.assertEqual(2, len(arguments.GetChildren()))
    self.assertEqual('Argument', arguments.GetChildren()[0].GetClass())
    self.assertEqual('x', arguments.GetChildren()[0].GetName())
    self.assertEqual('Argument', arguments.GetChildren()[1].GetClass())
    self.assertEqual('y', arguments.GetChildren()[1].GetName())

  def testNamedArgumentList(self):
    extended_attribute_text = '[NamedConstructor=Image(DOMString src)]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()))
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('NamedConstructor',attribute.GetName())
    self.assertEqual(1, len(attribute.GetChildren()))
    self.assertEqual('Call', attribute.GetChildren()[0].GetClass())
    self.assertEqual('Image', attribute.GetChildren()[0].GetName())
    arguments = attribute.GetChildren()[0].GetChildren()[0]
    self.assertEqual('Arguments', arguments.GetClass())
    self.assertEqual(1, len(arguments.GetChildren()))
    self.assertEqual('Argument', arguments.GetChildren()[0].GetClass())
    self.assertEqual('src', arguments.GetChildren()[0].GetName())
    argument = arguments.GetChildren()[0]
    self.assertEqual(1, len(argument.GetChildren()))
    self.assertEqual('Type', argument.GetChildren()[0].GetClass())
    arg = argument.GetChildren()[0]
    self.assertEqual(1, len(arg.GetChildren()))
    argType = arg.GetChildren()[0]
    self.assertEqual('StringType', argType.GetClass())
    self.assertEqual('DOMString', argType.GetName())

  def testIdentifier(self):
    extended_attribute_text = '[PutForwards=name]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()))
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('PutForwards', attribute.GetName())
    identifier = attribute.GetProperty('VALUE')
    self.assertEqual('name', identifier)

  def testIdentifierList(self):
    extended_attribute_text = '[Exposed=(Window,Worker)]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()))
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('Exposed', attribute.GetName())
    identifierList = attribute.GetProperty('VALUE')
    self.assertEqual(2, len(identifierList))
    self.assertEqual('Window', identifierList[0])
    self.assertEqual('Worker', identifierList[1])

  def testCombinationOfExtendedAttributes(self):
    extended_attribute_text = '[Replacable, Exposed=(Window,Worker)]'
    attributes = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(2, len(attributes.GetChildren()))
    attribute0 = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute0.GetClass())
    self.assertEqual('Replacable', attribute0.GetName())
    attribute1 = attributes.GetChildren()[1]
    self.assertEqual('ExtAttribute', attribute1.GetClass())
    self.assertEqual('Exposed', attribute1.GetName())
    identifierList = attribute1.GetProperty('VALUE')
    self.assertEqual(2, len(identifierList))
    self.assertEqual('Window', identifierList[0])
    self.assertEqual('Worker', identifierList[1])

  def testErrorTrailingComma(self):
    extended_attribute_text = '[Replacable, Exposed=(Window,Worker),]'
    error = self._ParseIdlWithExtendedAttributes(extended_attribute_text)
    self.assertEqual('Error', error.GetClass())
    error_message = error.GetName()
    self.assertEqual('Unexpected "]" after ",".', error_message)
    self.assertEqual('ExtendedAttributeList', error.GetProperty('PROD'))

  def testErrorMultipleExtendedAttributes(self):
    extended_attribute_text = '[Attribute1][Attribute2]'
    idl_text = extended_attribute_text + ' enum MealType {"rice"};'
    filenode = self.parser.ParseText(filename='', data=idl_text)
    self.assertEqual(1, len(filenode.GetChildren()))
    node = filenode.GetChildren()[0]
    self.assertEqual('Error', node.GetClass())
    self.assertEqual('Unexpected "[" after "]".', node.GetName())
    self.assertEqual('Definition', node.GetProperty('PROD'))
    children = node.GetChildren()
    self.assertEqual(1, len(children))
    attributes = children[0]
    self.assertEqual('ExtAttributes', attributes.GetClass())
    self.assertEqual(1, len(attributes.GetChildren()))
    attribute = attributes.GetChildren()[0]
    self.assertEqual('ExtAttribute', attribute.GetClass())
    self.assertEqual('Attribute1', attribute.GetName())


class TestDefaultValue(unittest.TestCase):
  def setUp(self):
    self.parser = IDLParser(IDLLexer(), mute_error=True)

  def _ParseDefaultValue(self, default_value_text):
    idl_text = 'interface I { void hello(' + default_value_text + '); };'
    filenode = self.parser.ParseText(filename='', data=idl_text)
    self.assertEqual(1, len(filenode.GetChildren()))
    node = filenode.GetChildren()[0]
    self.assertEqual('Interface', node.GetClass())
    self.assertEqual('I', node.GetName())
    children = node.GetChildren()
    self.assertEqual(1, len(children))
    operation = children[0]
    self.assertEqual('Operation', operation.GetClass())
    self.assertEqual('hello', operation.GetName())
    self.assertEqual(2, len(operation.GetChildren()))
    arguments = operation.GetChildren()[0]
    self.assertEqual('Arguments', arguments.GetClass())
    self.assertEqual(1, len(arguments.GetChildren()))
    argument = arguments.GetChildren()[0]
    return_type = operation.GetChildren()[1]
    self._CheckTypeNode(return_type, 'PrimitiveType', 'void')
    return argument

  def _CheckTypeNode(self, type_node, expected_class, expected_name):
    self.assertEqual('Type', type_node.GetClass())
    self.assertEqual(1, len(type_node.GetChildren()))
    type_detail = type_node.GetChildren()[0]
    class_name = type_detail.GetClass()
    name = type_detail.GetName()
    self.assertEqual(expected_class, class_name)
    self.assertEqual(expected_name, name)

  def _CheckArgumentNode(self, argument, expected_class, expected_name):
    class_name = argument.GetClass()
    name = argument.GetName()
    self.assertEqual(expected_class, class_name)
    self.assertEqual(expected_name, name)

  def _CheckDefaultValue(self, default_value, expected_type, expected_value):
    self.assertEqual('Default', default_value.GetClass())
    self.assertEqual(expected_type, default_value.GetProperty('TYPE'))
    self.assertEqual(expected_value, default_value.GetProperty('VALUE'))

  def testDefaultValueDOMString(self):
    default_value_text = 'optional DOMString arg = "foo"'
    argument = self._ParseDefaultValue(default_value_text)
    self._CheckArgumentNode(argument, 'Argument', 'arg')
    argument_type = argument.GetChildren()[0]
    self._CheckTypeNode(argument_type, 'StringType', 'DOMString')
    default_value = argument.GetChildren()[1]
    self._CheckDefaultValue(default_value, 'DOMString', 'foo')

  def testDefaultValueInteger(self):
    default_value_text = 'optional long arg = 10'
    argument = self._ParseDefaultValue(default_value_text)
    self._CheckArgumentNode(argument, 'Argument', 'arg')
    argument_type = argument.GetChildren()[0]
    self._CheckTypeNode(argument_type, 'PrimitiveType', 'long')
    default_value = argument.GetChildren()[1]
    self._CheckDefaultValue(default_value, 'integer', '10')

  def testDefaultValueFloat(self):
    default_value_text = 'optional float arg = 1.5'
    argument = self._ParseDefaultValue(default_value_text)
    self._CheckArgumentNode(argument, 'Argument', 'arg')
    argument_type = argument.GetChildren()[0]
    self._CheckTypeNode(argument_type, 'PrimitiveType', 'float')
    default_value = argument.GetChildren()[1]
    self._CheckDefaultValue(default_value, 'float', '1.5')

  def testDefaultValueBoolean(self):
    default_value_text = 'optional boolean arg = true'
    argument = self._ParseDefaultValue(default_value_text)
    self._CheckArgumentNode(argument, 'Argument', 'arg')
    argument_type = argument.GetChildren()[0]
    self._CheckTypeNode(argument_type, 'PrimitiveType', 'boolean')
    default_value = argument.GetChildren()[1]
    self._CheckDefaultValue(default_value, 'boolean', True)

  def testDefaultValueNull(self):
    # Node is a nullable type
    default_value_text = 'optional Node arg = null'
    argument = self._ParseDefaultValue(default_value_text)
    self._CheckArgumentNode(argument, 'Argument', 'arg')
    argument_type = argument.GetChildren()[0]
    self._CheckTypeNode(argument_type, 'Typeref', 'Node')
    default_value = argument.GetChildren()[1]
    self._CheckDefaultValue(default_value, 'NULL', 'NULL')

if __name__ == '__main__':
  unittest.main(verbosity=2)
