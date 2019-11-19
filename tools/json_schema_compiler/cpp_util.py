# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilies and constants specific to Chromium C++ code.
"""

from code import Code
from datetime import datetime
from model import PropertyType
import os
import re

CHROMIUM_LICENSE = (
"""// Copyright (c) %d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.""" % datetime.now().year
)
GENERATED_FILE_MESSAGE = """// GENERATED FROM THE API DEFINITION IN
//   %s
// DO NOT EDIT.
"""
GENERATED_BUNDLE_FILE_MESSAGE = """// GENERATED FROM THE API DEFINITIONS IN
//   %s
// DO NOT EDIT.
"""
GENERATED_FEATURE_MESSAGE = """// GENERATED FROM THE FEATURE DEFINITIONS IN
//   %s
// DO NOT EDIT.
"""

def Classname(s):
  """Translates a namespace name or function name into something more
  suited to C++.

  eg experimental.downloads -> Experimental_Downloads
  updateAll -> UpdateAll.
  """
  if s == '':
    return 'EMPTY_STRING'
  return '_'.join([x[0].upper() + x[1:] for x in re.split(r'\W', s)])


def GetAsFundamentalValue(type_, src, dst):
  """Returns the C++ code for retrieving a fundamental type from a
  Value into a variable.

  src: Value*
  dst: Property*
  """
  return {
      PropertyType.BOOLEAN: '%s->GetAsBoolean(%s)',
      PropertyType.DOUBLE: '%s->GetAsDouble(%s)',
      PropertyType.INTEGER: '%s->GetAsInteger(%s)',
      PropertyType.STRING: '%s->GetAsString(%s)',
  }[type_.property_type] % (src, dst)


def GetValueType(type_):
  """Returns the Value::Type corresponding to the model.Type.
  """
  return {
      PropertyType.ARRAY: 'base::Value::Type::LIST',
      PropertyType.BINARY: 'base::Value::Type::BINARY',
      PropertyType.BOOLEAN: 'base::Value::Type::BOOLEAN',
      # PropertyType.CHOICES can be any combination of types.
      PropertyType.DOUBLE: 'base::Value::Type::DOUBLE',
      PropertyType.ENUM: 'base::Value::Type::STRING',
      PropertyType.FUNCTION: 'base::Value::Type::DICTIONARY',
      PropertyType.INTEGER: 'base::Value::Type::INTEGER',
      PropertyType.OBJECT: 'base::Value::Type::DICTIONARY',
      PropertyType.STRING: 'base::Value::Type::STRING',
  }[type_.property_type]


def GetParameterDeclaration(param, type_):
  """Gets a parameter declaration of a given model.Property and its C++
  type.
  """
  if param.type_.property_type in (PropertyType.ANY,
                                   PropertyType.ARRAY,
                                   PropertyType.BINARY,
                                   PropertyType.CHOICES,
                                   PropertyType.OBJECT,
                                   PropertyType.REF,
                                   PropertyType.STRING):
    arg = 'const %(type)s& %(name)s'
  else:
    arg = '%(type)s %(name)s'
  return arg % {
    'type': type_,
    'name': param.unix_name,
  }


def GenerateIfndefName(file_path):
  """Formats |file_path| as a #define name. Presumably |file_path| is a header
  file, or there's little point in generating a #define for it.

  e.g chrome/extensions/gen/file.h becomes CHROME_EXTENSIONS_GEN_FILE_H__.
  """
  return (('%s__' % file_path).upper()
      .replace('\\', '_')
      .replace('/', '_')
      .replace('.', '_'))


def OpenNamespace(cpp_namespace):
  """Get opening root namespace declarations.
  """
  c = Code()
  for component in cpp_namespace.split('::'):
    c.Append('namespace %s {' % component)
  return c


def CloseNamespace(cpp_namespace):
  """Get closing root namespace declarations.
  """
  c = Code()
  for component in reversed(cpp_namespace.split('::')):
    c.Append('}  // namespace %s' % component)
  return c


def ConstantName(feature_name):
  """Returns a kName for a feature's name.
  """
  return ('k' + ''.join(word[0].upper() + word[1:]
      for word in feature_name.replace('.', ' ').split()))


def CamelCase(unix_name):
  return ''.join(word.capitalize() for word in unix_name.split('_'))


def ClassName(filepath):
  return CamelCase(os.path.split(filepath)[1])


def GetCppNamespace(pattern, namespace):
  '''Returns the C++ namespace given |pattern| which includes a %(namespace)s
  substitution, and the |namespace| to substitute. It is expected that |pattern|
  has been passed as a flag to compiler.py from GYP/GN.
  '''
  # For some reason Windows builds escape the % characters, so unescape them.
  # This means that %% can never appear legitimately within a pattern, but
  # that's ok. It should never happen.
  cpp_namespace = pattern.replace('%%', '%') % { 'namespace': namespace }
  assert '%' not in cpp_namespace, \
         ('Did not manage to fully substitute namespace "%s" into pattern "%s"'
           % (namespace, pattern))
  return cpp_namespace
