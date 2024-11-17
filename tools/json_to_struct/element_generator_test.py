#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from element_generator import GenerateFieldContent
from element_generator import GenerateElements
import unittest

class ElementGeneratorTest(unittest.TestCase):
  def testGenerateIntFieldContent(self):
    lines = [];
    GenerateFieldContent('', {'type': 'int', 'default': 5}, None, lines, '  ',
                         {})
    self.assertEquals(['  5,'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'int', 'default': 5}, 12, lines, '  ', {})
    self.assertEquals(['  12,'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'int'}, -3, lines, '  ', {})
    self.assertEquals(['  -3,'], lines)

  def testGenerateStringFieldContent(self):
    lines = [];
    GenerateFieldContent('', {'type': 'string', 'default': 'foo_bar'}, None,
                         lines, '  ', {})
    self.assertEquals(['  "foo_bar",'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'string', 'default': 'foo'}, 'bar\n',
                         lines, '  ', {})
    self.assertEquals(['  "bar\\n",'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'string'}, None, lines, '  ', {})
    self.assertEquals(['  nullptr,'], lines)
    lines = []
    GenerateFieldContent('', {'type': 'string'}, 'foo', lines, '  ', {})
    self.assertEquals(['  "foo",'], lines)

  def testGenerateString16FieldContent(self):
    lines = [];
    GenerateFieldContent('', {'type': 'string16',
                              'default': u'f\u00d8\u00d81a'},
                         None, lines, '  ', {})
    self.assertEquals(['  u"f\\x00d8" u"\\x00d8" u"1a",'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'string16', 'default': 'foo'},
                         u'b\uc3a5r', lines, '  ', {})
    self.assertEquals(['  u"b\\xc3a5" u"r",'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'string16'}, None, lines, '  ', {})
    self.assertEquals(['  nullptr,'], lines)
    lines = []
    GenerateFieldContent('', {'type': 'string16'}, u'foo\\u1234', lines, '  ',
                         {})
    self.assertEquals(['  u"foo\\\\u1234",'], lines)

  def testGenerateEnumFieldContent(self):
    lines = [];
    GenerateFieldContent('', {'type': 'enum', 'default': 'RED'}, None, lines,
                         '  ', {})
    self.assertEquals(['  RED,'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'enum', 'default': 'RED'}, 'BLACK', lines,
                         '  ', {})
    self.assertEquals(['  BLACK,'], lines)
    lines = [];
    GenerateFieldContent('', {'type': 'enum'}, 'BLUE', lines, '  ', {})
    self.assertEquals(['  BLUE,'], lines)

  def testGenerateClassFieldContent(self):
    lines = []
    GenerateFieldContent('', {
        'type': 'class',
        'default': 'std::nullopt'
    }, None, lines, '  ', {})
    self.assertEquals(['  std::nullopt,'], lines)
    lines = []
    GenerateFieldContent('', {
        'type': 'class',
        'default': 'std::nullopt'
    }, 'true', lines, '  ', {})
    self.assertEquals(['  true,'], lines)
    lines = []
    GenerateFieldContent('', {'type': 'class'}, 'false', lines, '  ', {})
    self.assertEquals(['  false,'], lines)

  def testGenerateArrayFieldContent(self):
    lines = ['STRUCT BEGINS'];
    GenerateFieldContent('test', {'type': 'array', 'contents': {'type': 'int'}},
                         None, lines, '  ', {})
    self.assertEquals(['STRUCT BEGINS', '  {},'], lines)
    lines = ['STRUCT BEGINS']
    GenerateFieldContent('test', {
        'field': 'my_array',
        'type': 'array',
        'contents': {
            'type': 'int'
        }
    }, [3, 4], lines, '  ', {})
    self.assertEquals(
        'const int array_test_my_array[] = {\n' + '  3,\n' + '  4,\n' + '};\n' +
        'STRUCT BEGINS\n' + '  array_test_my_array,', '\n'.join(lines))
    lines = ['STRUCT BEGINS']
    GenerateFieldContent('test', {
        'field': 'my_array',
        'type': 'array',
        'contents': {
            'type': 'int'
        }
    }, [3, 4], lines, '  ', {'array_test_my_array': 1})
    self.assertEquals(
        'const int array_test_my_array_1[] = {\n' + '  3,\n' + '  4,\n' +
        '};\n' + 'STRUCT BEGINS\n' + '  array_test_my_array_1,',
        '\n'.join(lines))

  def testGenerateElements(self):
    schema = [
      {'field': 'f0', 'type': 'int', 'default': 1000, 'optional': True},
      {'field': 'f1', 'type': 'string'},
      {'field': 'f2', 'type': 'enum', 'ctype': 'QuasiBool', 'default': 'MAYBE',
       'optional': True},
      {'field': 'f3', 'type': 'array', 'contents': {'type': 'string16'},
       'optional': True},
      {
        'field': 'f4',
        'type': 'struct',
        'type_name': 'InnerType',
        'fields': [
          {'field': 'g0', 'type': 'string'}
        ],
        'optional': True
      },
      {
        'field': 'f5',
        'type': 'array',
        'contents': {
          'type': 'struct',
          'type_name': 'InnerType',
          'fields': [
            {'field': 'a0', 'type': 'string'},
            {'field': 'a1', 'type': 'string'}
          ]
        },
        'optional': True
      }
    ]
    description = {
      'int_variables': {'a': -5, 'b': 5},
      'elements': {
        'elem0': {'f0': 5, 'f1': 'foo', 'f2': 'SURE'},
        'elem1': {'f2': 'NOWAY', 'f0': -2, 'f1': 'bar'},
        'elem2': {'f1': 'foo_bar', 'f3': [u'bar', u'foo']},
        'elem3': {'f1': 'foo', 'f4': {'g0': 'test'}},
        'elem4': {'f1': 'foo', 'f5': [{'a0': 'test0', 'a1': 'test1'}]},
      }
    }

    # Build the expected result stream based on the unpredicatble order the
    # dictionary element are listed in.
    int_variable_expected = {
      'a': 'const int a = -5;\n',
      'b': 'const int b = 5;\n',
    }
    elements_expected = {
        'elem0':
        'const MyType elem0 = {\n'
        '  5,\n'
        '  "foo",\n'
        '  SURE,\n'
        '  {},\n'
        '  {0},\n'
        '  {},\n'
        '};\n',
        'elem1':
        'const MyType elem1 = {\n'
        '  -2,\n'
        '  "bar",\n'
        '  NOWAY,\n'
        '  {},\n'
        '  {0},\n'
        '  {},\n'
        '};\n',
        'elem2':
        'const char16_t* const array_elem2_f3[] = {\n'
        '  u"bar",\n'
        '  u"foo",\n'
        '};\n'
        'const MyType elem2 = {\n'
        '  1000,\n'
        '  "foo_bar",\n'
        '  MAYBE,\n'
        '  array_elem2_f3,\n'
        '  {0},\n'
        '  {},\n'
        '};\n',
        'elem3':
        'const MyType elem3 = {\n'
        '  1000,\n'
        '  "foo",\n'
        '  MAYBE,\n'
        '  {},\n'
        '  {\n'
        '    "test",\n'
        '  },\n'
        '  {},\n'
        '};\n',
        'elem4':
        'const InnerType array_elem4_f5[] = {\n'
        '  {\n'
        '    "test0",\n'
        '    "test1",\n'
        '  },\n'
        '};\n'
        'const MyType elem4 = {\n'
        '  1000,\n'
        '  "foo",\n'
        '  MAYBE,\n'
        '  {},\n'
        '  {0},\n'
        '  array_elem4_f5,\n'
        '};\n'
    }
    expected = ''
    for key, value in description['int_variables'].items():
      expected += int_variable_expected[key]
    expected += '\n'
    elements = []
    for key, value in description['elements'].items():
      elements.append(elements_expected[key])
    expected += '\n'.join(elements)

    result = GenerateElements('MyType', schema, description)
    self.assertEquals(expected, result)

  def testGenerateElementsMissingMandatoryField(self):
    schema = [
      {'field': 'f0', 'type': 'int'},
      {'field': 'f1', 'type': 'string'},
    ]
    description = {
      'int_variables': {'a': -5, 'b': 5},
      'elements': {
        'elem0': {'f0': 5},
      }
    }

    self.assertRaises(RuntimeError,
      lambda: GenerateElements('MyType', schema, description))

if __name__ == '__main__':
  unittest.main()
