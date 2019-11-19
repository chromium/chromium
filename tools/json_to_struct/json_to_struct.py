#!/usr/bin/env python
# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Format for the JSON schema file:
# {
#   "type_name": "DesiredCStructName",
#   "headers": [          // Optional list of headers to be included by the .h.
#     "path/to/header.h"
#   ],
#   "schema": [           // Fields of the generated structure.
#     {
#       "field": "my_enum_field",
#       "type": "enum",   // Either: int, string, string16, enum, array, struct.
#       "default": "RED", // Optional. Cannot be used for array.
#       "ctype": "Color"  // Only for enum, specify the C type.
#     },
#     {
#       "field": "my_int_array_field",  // my_int_array_field_size will also
#       "type": "array",                // be generated.
#       "contents": {
#         "type": "int"   // Either: int, string, string16, enum, array.
#       }
#     },
#     {
#       "field": "my_struct_field",
#       "type_name": "PointStuct",
#       "type": "struct",
#       "fields": [
#         {"field": "x", "type": "int"},
#         {"field": "y", "type": "int"}
#       ]
#     },
#     ...
#   ]
# }
#
# Format for the JSON description file:
# {
#   "int_variables": {    // An optional list of constant int variables.
#     "kDesiredConstantName": 45
#   },
#   "elements": {         // All the elements for which to create static
#                         // initialization code in the .cc file.
#     "my_const_variable": {
#       "my_int_field": 10,
#       "my_string_field": "foo bar",
#       "my_enum_field": "BLACK",
#       "my_int_array_field": [ 1, 2, 3, 5, 7 ],
#       "my_struct_field": {"x": 1, "y": 2}
#     },
#     "my_other_const_variable": {
#       ...
#     }
#   }
# }

import json
from datetime import datetime
import os.path
import sys
import optparse
import re
_script_path = os.path.realpath(__file__)

sys.path.insert(0, os.path.normpath(_script_path + "/../../json_comment_eater"))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)

import struct_generator
import element_generator

HEAD = """// Copyright %d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE SCHEMA DEFINITION AND DESCRIPTION IN
//   %s
//   %s
// DO NOT EDIT.

"""

def _GenerateHeaderGuard(h_filename):
  """Generates the string used in #ifndef guarding the header file.
  """
  result = re.sub('[%s\\\\.]' % os.sep, '_', h_filename.upper())
  return re.sub('^_*', '', result) + '_'  # Remove leading underscores.

def _GenerateH(basepath, fileroot, head, namespace, schema, description):
  """Generates the .h file containing the definition of the structure specified
  by the schema.

  Args:
    basepath: The base directory in which files are generated.
    fileroot: The filename and path, relative to basepath, of the file to
        create, without an extension.
    head: The string to output as the header of the .h file.
    namespace: A string corresponding to the C++ namespace to use.
    schema: A dict containing the schema. See comment at the top of this file.
    description: A dict containing the description. See comment at the top of
        this file.
  """

  h_filename = fileroot + '.h'
  with open(os.path.join(basepath, h_filename), 'w') as f:
    f.write(head)

    header_guard = _GenerateHeaderGuard(h_filename)
    f.write('#ifndef %s\n' % header_guard)
    f.write('#define %s\n' % header_guard)
    f.write('\n')

    f.write('#include <cstddef>\n')
    f.write('\n')

    for header in schema.get('headers', []):
      f.write('#include "%s"\n' % header)
    f.write('\n')

    if namespace:
      f.write('namespace %s {\n' % namespace)
      f.write('\n')

    f.write(struct_generator.GenerateStruct(
      schema['type_name'], schema['schema']))
    f.write('\n')

    for var_name, value in description.get('int_variables', {}).items():
      f.write('extern const int %s;\n' % var_name)
    f.write('\n')

    for element_name, element in description['elements'].items():
      f.write('extern const %s %s;\n' % (schema['type_name'], element_name))

    if 'generate_array' in description:
      f.write('\n')
      f.write('extern const %s* const %s[];\n' % (schema['type_name'],
          description['generate_array']['array_name']))
      f.write('extern const size_t %s;\n' %
          (description['generate_array']['array_name'] + 'Length'))

    if namespace:
      f.write('\n')
      f.write('}  // namespace %s\n' % namespace)

    f.write('\n')
    f.write( '#endif  // %s\n' % header_guard)

def _GenerateCC(basepath, fileroot, head, namespace, schema, description):
  """Generates the .cc file containing the static initializers for the
  of the elements specified in the description.

  Args:
    basepath: The base directory in which files are generated.
    fileroot: The filename and path, relative to basepath, of the file to
        create, without an extension.
    head: The string to output as the header of the .cc file.
    namespace: A string corresponding to the C++ namespace to use.
    schema: A dict containing the schema. See comment at the top of this file.
    description: A dict containing the description. See comment at the top of
        this file.
  """

  with open(os.path.join(basepath, fileroot + '.cc'), 'w') as f:
    f.write(head)

    f.write('#include "%s"\n' % (fileroot + '.h'))
    f.write('\n')

    if namespace:
      f.write('namespace %s {\n' % namespace)
      f.write('\n')

    f.write(element_generator.GenerateElements(schema['type_name'],
        schema['schema'], description))

    if 'generate_array' in description:
      f.write('\n')
      f.write('const %s* const %s[] = {\n' % (schema['type_name'],
          description['generate_array']['array_name']))
      for element_name, _ in description['elements'].items():
        f.write('\t&%s,\n' % element_name)
      f.write('};\n')
      f.write('const size_t %s = %d;\n' %
          (description['generate_array']['array_name'] + 'Length',
           len(description['elements'])))

    if namespace:
      f.write('\n')
      f.write('}  // namespace %s\n' % namespace)

def _Load(filename):
  """Loads a JSON file int a Python object and return this object.
  """
  # TODO(beaudoin): When moving to Python 2.7 use object_pairs_hook=OrderedDict.
  with open(filename, 'r') as handle:
    result = json.loads(json_comment_eater.Nom(handle.read()))
  return result

def GenerateStruct(basepath, output_root, namespace, schema, description,
                   description_filename, schema_filename, year=None):
  """Generates a C++ struct from a JSON description.

  Args:
    basepath: The base directory in which files are generated.
    output_root: The filename and path, relative to basepath, of the file to
        create, without an extension.
    namespace: A string corresponding to the C++ namespace to use.
    schema: A dict containing the schema. See comment at the top of this file.
    description: A dict containing the description. See comment at the top of
        this file.
    description_filename: The description filename. This is added to the
        header of the outputted files.
    schema_filename: The schema filename. This is added to the header of the
        outputted files.
    year: Year to display next to the copy-right in the header.
  """
  year = int(year) if year else datetime.now().year
  head = HEAD % (year, schema_filename, description_filename)
  _GenerateH(basepath, output_root, head, namespace, schema, description)
  _GenerateCC(basepath, output_root, head, namespace, schema, description)

if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates an C++ array of struct from a JSON description.',
      usage='usage: %prog [option] -s schema description')
  parser.add_option('-b', '--destbase',
      help='base directory of generated files.')
  parser.add_option('-d', '--destdir',
      help='directory to output generated files, relative to destbase.')
  parser.add_option('-n', '--namespace',
      help='C++ namespace for generated files. e.g search_providers.')
  parser.add_option('-s', '--schema', help='path to the schema file, '
      'mandatory.')
  parser.add_option('-o', '--output', help='output filename, ')
  (opts, args) = parser.parse_args()

  if not opts.schema:
    parser.error('You must specify a --schema.')

  description_filename = os.path.normpath(args[0])
  root, ext = os.path.splitext(description_filename)
  shortroot = opts.output if opts.output else os.path.split(root)[1]
  if opts.destdir:
    output_root = os.path.join(os.path.normpath(opts.destdir), shortroot)
  else:
    output_root = shortroot

  if opts.destbase:
    basepath = os.path.normpath(opts.destbase)
  else:
    basepath = ''

  schema = _Load(opts.schema)
  description = _Load(description_filename)
  GenerateStruct(basepath, output_root, opts.namespace, schema, description,
                 description_filename, opts.schema)
