# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import class_generator

_INDENT = '    '


def _GenerateString(content, indent='  '):
  """Generates an UTF-8 string to be included in a static structure initializer.
  If content is not specified, uses NULL.
  """
  if content is None:
    return indent + 'null,'
  else:
    # json.dumps quotes the string and escape characters as required.
    return indent + '%s,' % json.dumps(content)


def _GenerateArrayVariableName(element_name, field_name, field_name_count):
  """Generates a unique variable name for an array variable.
  """
  var = '%s_%s' % (element_name, field_name)
  if var not in field_name_count:
    field_name_count[var] = 0
    return var
  new_var = '%s_%d' % (var, field_name_count[var])
  field_name_count[var] += 1
  return new_var


def _GenerateArray(element_name, field_info, content, indent, field_name_count):
  """Generates an array created inline in a constructor call. If content is
  not specified, null is used.
  """
  if content is None:
    return indent + 'null,'

  lines = []

  array_field = class_generator.GenerateField(field_info['contents'])
  array_type = array_field[:array_field.find(' ')]
  lines.append(indent + 'new %s[]{' % array_type)
  for subcontent in content:
    lines.append(
        _GenerateFieldContent(element_name, field_info['contents'], subcontent,
                              indent + _INDENT, field_name_count))
  lines.append(indent + '},')
  return '\n'.join(lines)


def _GenerateClass(element_name, field_info, content, indent, field_name_count):
  """Generates a class to be used in a constructor call. If content is not
  specified, use null.
  """
  if content is None:
    return indent + 'null'

  lines = []

  fields = field_info['fields']
  class_name = field_info['type_name']
  lines.append(indent + 'new ' + class_name + '(')
  for field in fields:
    subcontent = content.get(field['field'])
    lines.append(
        _GenerateFieldContent(element_name, field, subcontent, indent + _INDENT,
                              field_name_count))

  # remove the trailing comma for the last parameter
  lines[-1] = lines[-1][:-1]
  lines.append(indent + '),')

  return '\n'.join(lines)


def _GenerateFieldContent(element_name, field_info, content, indent,
                          field_name_count):
  """Generate the content of a field to be included in the constructor call. If
  the field's content is not specified, uses the default value if one exists.
  """
  if content is None:
    content = field_info.get('java_default', None)

  java_type = field_info['type']
  if java_type in ('int', 'class'):
    return '%s%s,' % (indent, content)
  elif java_type == 'enum':
    # TODO(peilinwang) temporarily treat enums as strings. Maybe use a
    # different schema? Right now these scripts are only used for generating
    # fieldtrial testing configs.
    return '%s"%s",' % (indent, content)
  elif java_type == 'string':
    return _GenerateString(content, indent)
  elif java_type == 'string16':
    raise RuntimeError('Generating a UTF16 java String is not supported yet.')
  elif java_type == 'array':
    return _GenerateArray(element_name, field_info, content, indent,
                          field_name_count)
  elif java_type == 'struct':
    return _GenerateClass(element_name, field_info, content, indent,
                          field_name_count)
  else:
    raise RuntimeError('Unknown field type "%s"' % java_type)


def _GenerateElement(type_name, schema, element_name, element,
                     field_name_count):
  """Generate the constructor call for one element.
  """
  lines = []
  lines.append(_INDENT + 'public static final %s %s = ' %
               (type_name, element_name))
  lines.append(2 * _INDENT + 'new %s(' % (type_name))
  for field_info in schema:
    content = element.get(field_info['field'], None)
    if (content == None and not field_info.get('optional', False)):
      raise RuntimeError('Mandatory field "%s" omitted in element "%s".' %
                         (field_info['field'], element_name))
    lines.append(
        _GenerateFieldContent(element_name, field_info, content, 2 * _INDENT,
                              field_name_count))

  # remove the trailing comma for the last parameter
  lines[-1] = lines[-1][:-1]
  lines.append(2 * _INDENT + ');')
  return '\n'.join(lines)


def GenerateElements(type_name, schema, description, field_name_count={}):
  """Generate the static initializers for all the elements in the
  description['elements'] dictionary, as well as for any variables in
  description['int_variables']. All elements in description['elements']
  will be generated as a public static final int/class.
  """
  result = []
  for var_name, value in description.get('int_variables', {}).items():
    result.append('public static final int %s = %s;' % (var_name, value))
  result.append('')

  for element_name, element in description.get('elements', {}).items():
    result.append(
        _GenerateElement(type_name, schema, element_name, element,
                         field_name_count))
    result.append('')
  return '\n'.join(result)
