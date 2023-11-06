# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_INDENT = '    '


def _GenerateArrayField(field_info):
  contents = field_info['contents']
  contents['field'] = '[] ' + field_info['field']
  if contents['type'] == 'array':
    raise RuntimeError('Nested arrays are not supported.')
  array_field = GenerateField(contents)
  array_field = array_field.replace(' []', '[]')
  return array_field


def _GenerateConstructorParams(fields, indent):
  lines = []
  for field in fields:
    field = field.replace(';', ',')
    parameter = indent + field
    if field != fields[-1]:
      parameter += ','
    lines.append(parameter)
  return '\n'.join(lines)


def _GenerateConstructorBody(fields, indent):
  lines = []
  for field in fields:
    field_name = field[field.find(' ') + 1:]
    lines.append('%sthis.%s = %s;' % (indent, field_name, field_name))
  return '\n'.join(lines)


def _GenerateConstructor(class_name, fields, indent):
  lines = []
  lines.append(indent + '%s(' % class_name)
  lines.append(_GenerateConstructorParams(fields, indent + _INDENT))
  lines.append(indent + ') {')
  lines.append(_GenerateConstructorBody(fields, indent + _INDENT))
  lines.append(indent + '}')
  return '\n'.join(lines)


def _GenerateClassFields(modifier, fields, indent):
  lines = []
  for field in fields:
    lines.append(indent + modifier + field + ';')
  return '\n'.join(lines)


def GenerateField(field_info):
  field = field_info['field']
  field_type = field_info['type']
  if field_type == 'int':
    return 'int %s' % field
  elif field_type == 'string' or field_type == 'string16':
    return 'String %s' % field
  elif field_type == 'enum' or field_type == 'class':
    return '%s %s' % (field_info['javatype'], field)
  elif field_type == 'array':
    return _GenerateArrayField(field_info)
  elif field_type == 'struct':
    return '%s %s' % (field_info['type_name'], field)
  else:
    raise RuntimeError('Unknown field type "%s"' % field_type)


def GenerateInnerClasses(type_name, schema):
  """Recursively generates the nested structures specified in the schema as
  inner classes. This generates all class members and constructors.
  """
  lines = []
  lines.append(_INDENT + 'static class %s {' % type_name)

  class_fields = []
  for field_info in schema:
    if field_info['type'] == 'struct':
      lines.insert(
          0, GenerateInnerClasses(field_info['type_name'],
                                  field_info['fields']))
    elif (field_info['type'] == 'array'
          and field_info['contents']['type'] == 'struct'):
      contents = field_info['contents']
      lines.insert(
          0, GenerateInnerClasses(contents['type_name'], contents['fields']))

    class_fields.append(GenerateField(field_info))

  lines.append(_GenerateConstructor(type_name, class_fields, 2 * _INDENT))
  modifier = 'public final '
  lines.append(_GenerateClassFields(modifier, class_fields, 2 * _INDENT))

  lines.append(_INDENT + '};')
  return '\n'.join(lines) + '\n'
