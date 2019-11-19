#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generator for C++ structs from api json files.

The purpose of this tool is to remove the need for hand-written code that
converts to and from base::Value types when receiving javascript api calls.
Originally written for generating code for extension apis. Reference schemas
are in chrome/common/extensions/api.

Usage example:
  compiler.py --root /home/Work/src --namespace extensions windows.json
    tabs.json
  compiler.py --destdir gen --root /home/Work/src
    --namespace extensions windows.json tabs.json
"""

from __future__ import print_function

import optparse
import os
import shlex
import sys

from cpp_bundle_generator import CppBundleGenerator
from cpp_generator import CppGenerator
from cpp_type_generator import CppTypeGenerator
from js_externs_generator import JsExternsGenerator
from js_interface_generator import JsInterfaceGenerator
import json_schema
from cpp_namespace_environment import CppNamespaceEnvironment
from model import Model
from namespace_resolver import NamespaceResolver
from schema_loader import SchemaLoader

# Names of supported code generators, as specified on the command-line.
# First is default.
GENERATORS = [
  'cpp', 'cpp-bundle-registration', 'cpp-bundle-schema', 'externs', 'interface'
]

def GenerateSchema(generator_name,
                   file_paths,
                   root,
                   destdir,
                   cpp_namespace_pattern,
                   bundle_name,
                   impl_dir,
                   include_rules):
  # Merge the source files into a single list of schemas.
  api_defs = []
  for file_path in file_paths:
    schema = os.path.relpath(file_path, root)
    api_def = SchemaLoader(root).LoadSchema(schema)

    # If compiling the C++ model code, delete 'nocompile' nodes.
    if generator_name == 'cpp':
      api_def = json_schema.DeleteNodes(api_def, 'nocompile')

    # Delete all 'nodefine' nodes. They are only for documentation.
    api_def = json_schema.DeleteNodes(api_def, 'nodefine')

    api_defs.extend(api_def)

  api_model = Model(allow_inline_enums=False)

  # For single-schema compilation make sure that the first (i.e. only) schema
  # is the default one.
  default_namespace = None

  # If we have files from multiple source paths, we'll use the common parent
  # path as the source directory.
  src_path = None

  # Load the actual namespaces into the model.
  for target_namespace, file_path in zip(api_defs, file_paths):
    relpath = os.path.relpath(os.path.normpath(file_path), root)
    namespace = api_model.AddNamespace(target_namespace,
                                       relpath,
                                       include_compiler_options=True,
                                       environment=CppNamespaceEnvironment(
                                           cpp_namespace_pattern))

    if default_namespace is None:
      default_namespace = namespace

    if src_path is None:
      src_path = namespace.source_file_dir
    else:
      src_path = os.path.commonprefix((src_path, namespace.source_file_dir))

    _, filename = os.path.split(file_path)
    filename_base, _ = os.path.splitext(filename)

  # Construct the type generator with all the namespaces in this model.
  schema_dir = os.path.dirname(os.path.relpath(file_paths[0], root))
  namespace_resolver = NamespaceResolver(root, schema_dir,
                                         include_rules, cpp_namespace_pattern)
  type_generator = CppTypeGenerator(api_model,
                                    namespace_resolver,
                                    default_namespace)
  if generator_name in ('cpp-bundle-registration', 'cpp-bundle-schema'):
    cpp_bundle_generator = CppBundleGenerator(root,
                                              api_model,
                                              api_defs,
                                              type_generator,
                                              cpp_namespace_pattern,
                                              bundle_name,
                                              src_path,
                                              impl_dir)
    if generator_name == 'cpp-bundle-registration':
      generators = [
        ('generated_api_registration.cc',
         cpp_bundle_generator.api_cc_generator),
        ('generated_api_registration.h', cpp_bundle_generator.api_h_generator),
      ]
    elif generator_name == 'cpp-bundle-schema':
      generators = [
        ('generated_schemas.cc', cpp_bundle_generator.schemas_cc_generator),
        ('generated_schemas.h', cpp_bundle_generator.schemas_h_generator)
      ]
  elif generator_name == 'cpp':
    cpp_generator = CppGenerator(type_generator)
    generators = [
      ('%s.h' % filename_base, cpp_generator.h_generator),
      ('%s.cc' % filename_base, cpp_generator.cc_generator)
    ]
  elif generator_name == 'externs':
    generators = [
      ('%s_externs.js' % namespace.unix_name, JsExternsGenerator())
    ]
  elif generator_name == 'interface':
    generators = [
      ('%s_interface.js' % namespace.unix_name, JsInterfaceGenerator())
    ]
  else:
    raise Exception('Unrecognised generator %s' % generator_name)

  output_code = []
  for filename, generator in generators:
    code = generator.Generate(namespace).Render()
    if destdir:
      if generator_name == 'cpp-bundle-registration':
        # Function registrations must be output to impl_dir, since they link in
        # API implementations.
        output_dir = os.path.join(destdir, impl_dir)
      else:
        output_dir = os.path.join(destdir, src_path)
      if not os.path.exists(output_dir):
        os.makedirs(output_dir)
      with open(os.path.join(output_dir, filename), 'w') as f:
        f.write(code)
    # If multiple files are being output, add the filename for each file.
    if len(generators) > 1:
      output_code += [filename, '', code, '']
    else:
      output_code += [code]

  return '\n'.join(output_code)


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Generates a C++ model of an API from JSON schema',
      usage='usage: %prog [option]... schema')
  parser.add_option('-r', '--root', default='.',
      help='logical include root directory. Path to schema files from specified'
      ' dir will be the include path.')
  parser.add_option('-d', '--destdir',
      help='root directory to output generated files.')
  parser.add_option('-n', '--namespace', default='generated_api_schemas',
      help='C++ namespace for generated files. e.g extensions::api.')
  parser.add_option('-b', '--bundle-name', default='',
      help='A string to prepend to generated bundle class names, so that '
           'multiple bundle rules can be used without conflicting. '
           'Only used with one of the cpp-bundle generators.')
  parser.add_option('-g', '--generator', default=GENERATORS[0],
      choices=GENERATORS,
      help='The generator to use to build the output code. Supported values are'
      ' %s' % GENERATORS)
  parser.add_option('-i', '--impl-dir', dest='impl_dir',
      help='The root path of all API implementations')
  parser.add_option('-I', '--include-rules',
      help='A list of paths to include when searching for referenced objects,'
      ' with the namespace separated by a \':\'. Example: '
      '/foo/bar:Foo::Bar::%(namespace)s')

  (opts, file_paths) = parser.parse_args()

  if not file_paths:
    sys.exit(0) # This is OK as a no-op

  # Unless in bundle mode, only one file should be specified.
  if (opts.generator not in ('cpp-bundle-registration', 'cpp-bundle-schema') and
      len(file_paths) > 1):
    # TODO(sashab): Could also just use file_paths[0] here and not complain.
    raise Exception(
        "Unless in bundle mode, only one file can be specified at a time.")

  def split_path_and_namespace(path_and_namespace):
    if ':' not in path_and_namespace:
      raise ValueError('Invalid include rule "%s". Rules must be of '
                       'the form path:namespace' % path_and_namespace)
    return path_and_namespace.split(':', 1)

  include_rules = []
  if opts.include_rules:
    include_rules = list(
        map(split_path_and_namespace, shlex.split(opts.include_rules)))

  result = GenerateSchema(opts.generator, file_paths, opts.root, opts.destdir,
                          opts.namespace, opts.bundle_name, opts.impl_dir,
                          include_rules)
  if not opts.destdir:
    print(result)
