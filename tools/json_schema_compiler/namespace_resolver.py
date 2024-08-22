# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from cpp_namespace_environment import CppNamespaceEnvironment
from model import Model, UnixName
from schema_loader import SchemaLoader


def _GenerateFilenames(full_namespace):
  # Try to find the file defining the namespace. Eg. for
  # nameSpace.sub_name_space.Type' the following heuristics looks for:
  # 1. name_space_sub_name_space.json,
  # 2. name_space_sub_name_space.idl,
  # 3. sub_name_space.json,
  # 4. sub_name_space.idl,
  # 5. etc.
  sub_namespaces = full_namespace.split('.')
  filenames = []
  basename = None
  for namespace in reversed(sub_namespaces):
    if basename is not None:
      basename = UnixName(namespace + '.' + basename)
    else:
      basename = UnixName(namespace)
    for ext in ['json', 'idl']:
      filenames.append('%s.%s' % (basename, ext))
  return filenames


class NamespaceResolver(object):
  '''Resolves a type name into the namespace the type belongs to.
  - |root| path to the root directory.
  - |path| path to the directory with the API header files, relative to the
    root.
  - |include_rules| List containing tuples with (path, cpp_namespace_pattern)
    used when searching for types.
  - |cpp_namespace_pattern| Default namespace pattern
  '''

  def __init__(self, root, path, include_rules, cpp_namespace_pattern):
    self._root = root
    self._include_rules = [(path, cpp_namespace_pattern)] + include_rules

  def ResolveNamespace(self, full_namespace):
    '''Returns the model.Namespace object associated with the |full_namespace|,
    or None if one can't be found.
    '''
    filenames = _GenerateFilenames(full_namespace)
    for path, cpp_namespace in self._include_rules:
      cpp_namespace_environment = None
      if cpp_namespace:
        cpp_namespace_environment = CppNamespaceEnvironment(cpp_namespace)
      for filename in reversed(filenames):
        filepath = os.path.join(path, filename)
        if os.path.exists(os.path.join(self._root, filepath)):
          schema = SchemaLoader(self._root).LoadSchema(filepath)[0]
          return Model().AddNamespace(schema,
                                      filepath,
                                      environment=cpp_namespace_environment)
    return None

  def ResolveType(self, full_name, default_namespace):
    '''Returns the model.Namespace object where the type with the given
    |full_name| is defined, or None if one can't be found.
    '''
    name_parts = full_name.rsplit('.', 1)
    if len(name_parts) == 1:
      if full_name not in default_namespace.types:
        return None
      return default_namespace
    full_namespace, type_name = full_name.rsplit('.', 1)
    namespace = self.ResolveNamespace(full_namespace)
    if namespace and type_name in namespace.types:
      return namespace
    return None
