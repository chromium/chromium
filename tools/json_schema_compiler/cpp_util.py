# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilies and constants specific to Chromium C++ code.
"""

from code import Code
from datetime import datetime
from model import PropertyType
import os
import posixpath
import re

CHROMIUM_LICENSE = (
"""// Copyright %d The Chromium Authors. All rights reserved.
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
  updateAll -> UpdateAll
  update_all -> UpdateAll
  """
  if s == '':
    return 'EMPTY_STRING'
  if IsUnixName(s):
    return CamelCase(s)
  return '_'.join([x[0].upper() + x[1:] for x in re.split(r'\W', s)])


def GetAsFundamentalValue(type_, src, dst):
  """Returns the C++ code for retrieving a fundamental type from a
  Value into a variable.

  src: Value*
  dst: Property*
  """
  if type_.property_type == PropertyType.BOOLEAN:
    s = '%s->GetAsBoolean(%s)'
  elif type_.property_type == PropertyType.DOUBLE:
    s = '%s->GetAsDouble(%s)'
  elif type_.property_type == PropertyType.INTEGER:
    s = '%s->GetAsInteger(%s)'
  elif (type_.property_type == PropertyType.STRING or
      (type_.property_type == PropertyType.FUNCTION and
           type_.is_serializable_function)):
    s = '%s->GetAsString(%s)'
  else:
    raise ValueError('Type %s is not a fundamental value' % type_.name)

  return s % (src, dst)


def GetValueType(type_):
  """Returns the Value::Type corresponding to the model.Type.
  """
  if type_.property_type == PropertyType.ARRAY:
    return 'base::Value::Type::LIST'
  if type_.property_type == PropertyType.BINARY:
    return 'base::Value::Type::BINARY'
  if type_.property_type == PropertyType.BOOLEAN:
    return 'base::Value::Type::BOOLEAN'
  if type_.property_type == PropertyType.DOUBLE:
    return 'base::Value::Type::DOUBLE'
  if type_.property_type == PropertyType.ENUM:
    return 'base::Value::Type::STRING'
  if type_.property_type == PropertyType.FUNCTION:
    if type_.is_serializable_function:
      return 'base::Value::Type::STRING'
    return 'base::Value::Type::DICTIONARY'
  if type_.property_type == PropertyType.INTEGER:
    return 'base::Value::Type::INTEGER'
  if type_.property_type == PropertyType.OBJECT:
    return 'base::Value::Type::DICTIONARY'
  if type_.property_type == PropertyType.STRING:
    return 'base::Value::Type::STRING'

  raise ValueError('Invalid type: %s' % type_.name)


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


def FeatureNameToConstantName(feature_name):
  # type: (str) -> str
  """Returns a kName for a feature's name.
  """
  return ('k' + ''.join(word[0].upper() + word[1:]
      for word in feature_name.replace('.', ' ').split()))


def UnixNameToConstantName(unix_name):
  # type (str) -> str
  """Converts unix_name to kUnixName.
  """
  return ('k' + ''.join(word.capitalize() for word in unix_name.split('_')))

def IsUnixName(s):
  # type (str) -> bool
  """Returns true if |s| is of the type unix_name i.e. only has lower cased
  characters and underscores with at least one underscore.
  """
  return all(x.islower() or x == '_' for x in s) and '_' in s

def ToPosixPath(path):
  """Returns |path| with separator converted to POSIX style.

  This is needed to generate C++ #include paths.
  """
  return path.replace(os.path.sep, posixpath.sep)


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
