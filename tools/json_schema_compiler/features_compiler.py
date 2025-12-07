#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator for C++ features from json files.

Usage example:
  features_compiler.py --destdir gen --root /home/Work/src _permissions.json
"""

import optparse
import os

from schema_loader import SchemaLoader
from features_cc_generator import CCGenerator
from features_h_generator import HGenerator
from model import CreateFeature


def _GenerateSchema(filename, root, destdir, namespace):
  """Generates C++ features files from the json file |filename|.
  """
  # Load in the feature permissions from the JSON file.
  schema = os.path.normpath(filename)
  schema_loader = SchemaLoader(os.path.dirname(os.path.relpath(schema, root)),
                               os.path.dirname(schema), [], None)
  schema_filename = os.path.splitext(schema)[0]
  feature_defs = schema_loader.LoadSchema(schema)

  # Generate a list of the features defined and a list of their models.
  feature_list = []
  for feature_def, feature in feature_defs.items():
    feature_list.append(CreateFeature(feature_def, feature))

  source_file_dir, _ = os.path.split(schema)
  relpath = os.path.relpath(os.path.normpath(source_file_dir), root)
  full_path = os.path.join(relpath, schema)

  generators = [('%s.cc' % schema_filename, CCGenerator()),
                ('%s.h' % schema_filename, HGenerator())]

  # Generate and output the code for all features.
  output_code = []
  for filename, generator in generators:
    code = generator.Generate(feature_list, full_path, namespace).Render()
    if destdir:
      with open(os.path.join(destdir, relpath, filename), 'w') as f:
        f.write(code)
    output_code += [filename, '', code, '']

  return '\n'.join(output_code)


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates a C++ features model from JSON schema',
      usage='usage: %prog [option]... schema')
  parser.add_option(
      '-r',
      '--root',
      default='.',
      help='logical include root directory. Path to schema files from '
      'specified dir will be the include path.')
  parser.add_option('-d',
                    '--destdir',
                    help='root directory to output generated files.')
  parser.add_option(
      '-n',
      '--namespace',
      default='generated_features',
      help='C++ namespace for generated files. e.g extensions::api.')
  (opts, filenames) = parser.parse_args()

  # Only one file is currently specified.
  if len(filenames) != 1:
    raise ValueError('One (and only one) file is required (for now).')

  result = _GenerateSchema(filenames[0], opts.root, opts.destdir,
                           opts.namespace)
  if not opts.destdir:
    print(result)
