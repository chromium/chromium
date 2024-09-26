# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import struct_generator

def _JSONToCString16(json_string_literal):
  """Converts a JSON string literal to a C++ UTF-16 string literal. This is
  done by converting \\u#### to \\x####.
  """
  c_string_literal = json_string_literal
  escape_index = c_string_literal.find('\\')
  while escape_index > 0:
    if c_string_literal[escape_index + 1] == 'u':
      # We close the C string literal after the 4 hex digits and reopen it right
      # after, otherwise the Windows compiler will sometimes try to get more
      # than 4 characters in the hex string.
      c_string_literal = (c_string_literal[0:escape_index + 1] + 'x' +
                          c_string_literal[escape_index + 2:escape_index + 6] +
                          '" u"' + c_string_literal[escape_index + 6:])
    escape_index = c_string_literal.find('\\', escape_index + 6)
  return c_string_literal

def _GenerateString(content, lines, indent='  '):
  """Generates an UTF-8 string to be included in a static structure initializer.
  If content is not specified, uses nullptr.
  """
  if content is None:
    lines.append(indent + 'nullptr,')
  else:
    # json.dumps quotes the string and escape characters as required.
    lines.append(indent + '%s,' % json.dumps(content))

def _GenerateString16(content, lines, indent='  '):
  """Generates an UTF-16 string to be included in a static structure
  initializer. If content is not specified, uses nullptr.
  """
  if content is None:
    lines.append(indent + 'nullptr,')
  else:
    # json.dumps quotes the string and escape characters as required.
    lines.append(indent + 'u%s,' % _JSONToCString16(json.dumps(content)))

def _GenerateArrayVariableName(element_name, field_name, field_name_count):
  # Generates a unique variable name for an array variable.
  var = 'array_%s_%s' % (element_name, field_name)
  if var not in field_name_count:
    field_name_count[var] = 0
    return var
  new_var = '%s_%d' % (var, field_name_count[var])
  field_name_count[var] += 1
  return new_var

def _GenerateArray(element_name, field_info, content, lines, indent,
                   field_name_count):
  """Generates an array to be included in a static structure initializer. If
  content is not specified, uses nullptr. The array is assigned to a temporary
  variable which is initialized before the structure.
  """
  if not content:
    lines.append(indent + '{},')
    return

  # Create a new array variable and use it in the structure initializer.
  # This prohibits nested arrays. Add a clash detection and renaming mechanism
  # to solve the problem.
  var = _GenerateArrayVariableName(element_name, field_info['field'],
                                   field_name_count)
  lines.append(indent + '%s,' % var)
  # Generate the array content.
  array_lines = []
  field_info['contents']['field'] = var;
  array_lines.append(struct_generator.GenerateField(
                     field_info['contents']) + '[] = {')
  for subcontent in content:
    GenerateFieldContent(element_name, field_info['contents'], subcontent,
                         array_lines, indent, field_name_count)
  array_lines.append('};')
  # Prepend the generated array so it is initialized before the structure.
  lines.reverse()
  array_lines.reverse()
  lines.extend(array_lines)
  lines.reverse()

def _GenerateStruct(element_name, field_info, content, lines, indent,
                    field_name_count):
  """Generates a struct to be included in a static structure initializer. If
  content is not specified, uses {0}.
  """
  if content is None:
    lines.append(indent + '{0},')
    return

  fields = field_info['fields']
  lines.append(indent + '{')
  for field in fields:
    subcontent = content.get(field['field'])
    GenerateFieldContent(element_name, field, subcontent, lines, '  ' + indent,
                         field_name_count)
  lines.append(indent + '},')

def GenerateFieldContent(element_name, field_info, content, lines, indent,
                         field_name_count):
  """Generate the content of a field to be included in the static structure
  initializer. If the field's content is not specified, uses the default value
  if one exists.
  """
  if content is None:
    content = field_info.get('default', None)
  type = field_info['type']
  if type in ('int', 'enum', 'class'):
    lines.append('%s%s,' % (indent, content))
  elif type == 'string':
    _GenerateString(content, lines, indent)
  elif type == 'string16':
    _GenerateString16(content, lines, indent)
  elif type == 'array':
    _GenerateArray(element_name, field_info, content, lines, indent,
                   field_name_count)
  elif type == 'struct':
    _GenerateStruct(element_name, field_info, content, lines, indent,
                    field_name_count)
  else:
    raise RuntimeError('Unknown field type "%s"' % type)

def GenerateElement(type_name, schema, element_name, element, field_name_count):
  """Generate the static structure initializer for one element.
  """
  lines = [];
  lines.append('const %s %s = {' % (type_name, element_name));
  for field_info in schema:
    content = element.get(field_info['field'], None)
    if (content == None and not field_info.get('optional', False)):
      raise RuntimeError('Mandatory field "%s" omitted in element "%s".' %
                         (field_info['field'], element_name))
    GenerateFieldContent(element_name, field_info, content, lines, '  ',
                         field_name_count)
  lines.append('};')
  return '\n'.join(lines)

def GenerateElements(type_name, schema, description, field_name_count={}):
  """Generate the static structure initializer for all the elements in the
  description['elements'] dictionary, as well as for any variables in
  description['int_variables'].
  """
  result = [];
  for var_name, value in description.get('int_variables', {}).items():
    result.append('const int %s = %s;' % (var_name, value))
  result.append('')

  for element_name, element in description.get('elements', {}).items():
    result.append(GenerateElement(type_name, schema, element_name, element,
                                  field_name_count))
    result.append('')
  return '\n'.join(result)
