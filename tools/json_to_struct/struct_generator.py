# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _GenerateArrayField(field_info):
  """Generate a string defining an array field in a C structure.
  """
  contents = field_info['contents']
  contents['field'] = ''  # Required by the GenerateField call.
  return f'const base::span<{GenerateField(contents)}> {field_info["field"]}'

def GenerateField(field_info):
  """Generate a string defining a field of the type specified by
  field_info['type'] in a C structure.
  """
  field = field_info['field']
  type = field_info['type']
  if type == 'int':
    return f'const int {field}'.strip()
  elif type == 'string':
    return f'const char* const {field}'.strip()
  elif type == 'string16':
    return f'const char16_t* const {field}'.strip()
  elif type == 'enum' or type == 'class':
    return f'const {field_info["ctype"]} {field}'.strip()
  elif type == 'array':
    return _GenerateArrayField(field_info)
  elif type == 'struct':
    return f'const {field_info["type_name"]} {field}'.strip()
  else:
    raise RuntimeError('Unknown field type "%s"' % type)

def GenerateStruct(type_name, schema):
  """Generate a string defining a structure containing the fields specified in
  the schema list.
  """
  lines = []
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
  lines.append('};')
  return '\n'.join(lines) + '\n'
