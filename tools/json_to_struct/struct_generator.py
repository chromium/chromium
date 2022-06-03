# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _GenerateArrayField(field_info):
  """Generate a string defining an array field in a C structure.
  """
  contents = field_info['contents']
  contents['field'] = '* ' + field_info['field']
  if contents['type'] == 'array':
    raise RuntimeError('Nested arrays are not supported.')
  return (GenerateField(contents) + ';\n' +
          '  const size_t %s_size') % field_info['field'];

def GenerateField(field_info):
  """Generate a string defining a field of the type specified by
  field_info['type'] in a C structure.
  """
  field = field_info['field']
  type = field_info['type']
  if type == 'int':
    return 'const int %s' % field
  elif type == 'string':
    return 'const char* const %s' % field
  elif type == 'string16':
    return 'const wchar_t* const %s' % field
  elif type == 'enum' or type == 'class':
    return 'const %s %s' % (field_info['ctype'], field)
  elif type == 'array':
    return _GenerateArrayField(field_info)
  elif type == 'struct':
    return 'const %s %s' % (field_info['type_name'], field)
  else:
    raise RuntimeError('Unknown field type "%s"' % type)

def GenerateStruct(type_name, schema):
  """Generate a string defining a structure containing the fields specified in
  the schema list.
  """
  lines = [];
  lines.append('struct %s {' % type_name)
  for field_info in schema:
    if field_info['type'] == 'struct':
      lines.insert(0, GenerateStruct(field_info['type_name'],
                                     field_info['fields']))
    elif (field_info['type'] == 'array'
          and field_info['contents']['type'] == 'struct'):
      contents = field_info['contents']
      lines.insert(0, GenerateStruct(contents['type_name'],
                                     contents['fields']))
    lines.append('  ' + GenerateField(field_info) + ';')
  lines.append('};');
  return '\n'.join(lines) + '\n';
