# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import code_util
import cpp_util
from model import Platforms
from schema_util import CapitalizeFirstLetter
from schema_util import JsFunctionNameToClassName

import collections
import copy
import json
import os
import re
import sys


def _RemoveKey(node, key, type_restriction):
  if isinstance(node, dict):
    if key in node and isinstance(node[key], type_restriction):
      del node[key]
    for value in node.values():
      _RemoveKey(value, key, type_restriction)
  elif isinstance(node, list):
    for value in node:
      _RemoveKey(value, key, type_restriction)


def _RemoveUnneededFields(schema):
  """Returns a copy of |schema| with fields that aren't necessary at runtime
  removed.
  """
  # Return a copy so that we don't pollute the global api object, which may be
  # used elsewhere.
  ret = copy.deepcopy(schema)
  _RemoveKey(ret, 'description', str)
  _RemoveKey(ret, 'compiler_options', dict)
  _RemoveKey(ret, 'nodoc', bool)
  _RemoveKey(ret, 'nocompile', bool)
  _RemoveKey(ret, 'noinline_doc', bool)
  _RemoveKey(ret, 'manifest_keys', object)
  return ret


def _PrefixSchemaWithNamespace(schema):
  """Modifies |schema| in place to prefix all types and references with a
  namespace, if they aren't already qualified. That is, in the tabs API, this
  will turn type Tab into tabs.Tab, but will leave the fully-qualified
  windows.Window as-is.
  """
  assert isinstance(schema, dict), "Schema is unexpected type"
  namespace = schema['namespace']

  def prefix(obj, key, mandatory):
    if not key in obj:
      assert not mandatory, ('Required key "%s" is not present in object.' %
                             key)
      return
    assert type(obj[key]) is str
    if obj[key].find('.') == -1:
      obj[key] = '%s.%s' % (namespace, obj[key])

  if 'types' in schema:
    assert isinstance(schema['types'], list)
    for t in schema['types']:
      assert isinstance(t, dict), "Type entry is unexpected type"
      prefix(t, 'id', True)
      prefix(t, 'customBindings', False)

  def prefix_refs(val):
    if type(val) is list:
      for sub_val in val:
        prefix_refs(sub_val)
    elif type(val) is dict or type(val) is collections.OrderedDict:
      prefix(val, '$ref', False)
      for key, sub_val in val.items():
        prefix_refs(sub_val)

  prefix_refs(schema)
  return schema


class CppBundleGenerator(object):
  """This class contains methods to generate code based on multiple schemas.
  """

  def __init__(self, root, model, api_defs, cpp_type_generator,
               cpp_namespace_pattern, bundle_name, source_file_dir, impl_dir):
    self._root = root
    self._model = model
    self._api_defs = api_defs
    self._cpp_type_generator = cpp_type_generator
    self._bundle_name = bundle_name
    self._source_file_dir = source_file_dir
    self._impl_dir = impl_dir

    # Hack: assume that the C++ namespace for the bundle is the namespace of the
    # files without the last component of the namespace. A cleaner way to do
    # this would be to make it a separate variable in the gyp file.
    self._cpp_namespace = cpp_namespace_pattern.rsplit('::', 1)[0]

    self.api_cc_generator = _APICCGenerator(self)
    self.api_h_generator = _APIHGenerator(self)
    self.schemas_cc_generator = _SchemasCCGenerator(self)
    self.schemas_h_generator = _SchemasHGenerator(self)

  def _GenerateHeader(self, file_base, body_code):
    """Generates a code_util.Code object for a header file

    Parameters:
    - |file_base| - the base of the filename, e.g. 'foo' (for 'foo.h')
    - |body_code| - the code to put in between the multiple inclusion guards"""
    c = code_util.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append(cpp_util.GENERATED_BUNDLE_FILE_MESSAGE %
             cpp_util.ToPosixPath(self._source_file_dir))
    ifndef_name = cpp_util.GenerateIfndefName(
        '%s/%s.h' % (cpp_util.ToPosixPath(self._source_file_dir), file_base))
    c.Append()
    c.Append('#ifndef %s' % ifndef_name)
    c.Append('#define %s' % ifndef_name)
    c.Append()
    c.Concat(body_code)
    c.Append()
    c.Append('#endif  // %s' % ifndef_name)
    c.Append()
    return c

  def _GetPlatformIfdefs(self, model_object):
    """Generates the "defined" conditional for an #if check if |model_object|
    has platform restrictions. Returns None if there are no restrictions.
    """
    if model_object.platforms is None:
      return None
    ifdefs = []
    for platform in model_object.platforms:
      if platform == Platforms.CHROMEOS:
        ifdefs.append('BUILDFLAG(IS_CHROMEOS_ASH)')
      elif platform == Platforms.FUCHSIA:
        ifdefs.append('BUILDFLAG(IS_FUCHSIA)')
      elif platform == Platforms.LACROS:
        # TODO(crbug.com/40118868): For readability, this should become
        # BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_LACROS).
        ifdefs.append('BUILDFLAG(IS_CHROMEOS_LACROS)')
      elif platform == Platforms.LINUX:
        ifdefs.append('BUILDFLAG(IS_LINUX)')
      elif platform == Platforms.MAC:
        ifdefs.append('BUILDFLAG(IS_MAC)')
      elif platform == Platforms.WIN:
        ifdefs.append('BUILDFLAG(IS_WIN)')
      else:
        raise ValueError("Unsupported platform ifdef: %s" % platform.name)
    return ' || '.join(ifdefs)

  def _GenerateRegistrationEntry(self, namespace_name, function):
    c = code_util.Code()
    function_ifdefs = self._GetPlatformIfdefs(function)
    if function_ifdefs is not None:
      c.Append("#if %s" % function_ifdefs, indent_level=0)

    function_name = '%sFunction' % JsFunctionNameToClassName(
        namespace_name, function.name)
    c.Sblock('{')
    c.Append('&NewExtensionFunction<%s>,' % function_name)
    c.Append('%s::static_function_name(),' % function_name)
    c.Append('%s::static_histogram_value(),' % function_name)
    c.Eblock('},')

    if function_ifdefs is not None:
      c.Append("#endif  // %s" % function_ifdefs, indent_level=0)
    return c

  def _GenerateFunctionRegistryRegisterAll(self):
    c = code_util.Code()
    c.Append('// static')
    c.Sblock('void %s::RegisterAll(ExtensionFunctionRegistry* registry) {' %
             self._GenerateBundleClass('GeneratedFunctionRegistry'))
    c.Sblock('constexpr ExtensionFunctionRegistry::FactoryEntry kEntries[] = {')
    for namespace in self._model.namespaces.values():
      namespace_ifdefs = self._GetPlatformIfdefs(namespace)
      if namespace_ifdefs is not None:
        c.Append("#if %s" % namespace_ifdefs, indent_level=0)

      for function in namespace.functions.values():
        if function.nocompile:
          continue
        c.Concat(self._GenerateRegistrationEntry(namespace.name, function))

      for type_ in namespace.types.values():
        for function in type_.functions.values():
          if function.nocompile:
            continue
          namespace_types_name = JsFunctionNameToClassName(
              namespace.name, type_.name)
          c.Concat(
              self._GenerateRegistrationEntry(namespace_types_name, function))

      if namespace_ifdefs is not None:
        c.Append("#endif  // %s" % namespace_ifdefs, indent_level=0)
    c.Eblock("};")
    c.Sblock("for (const auto& entry : kEntries) {")
    c.Append("  registry->Register(entry);")
    c.Eblock("}")
    c.Eblock("}")
    return c

  def _GenerateBundleClass(self, class_name):
    '''Generates the C++ class name to use for a bundle class, taking into
    account the bundle's name.
    '''
    return self._bundle_name + class_name


class _APIHGenerator(object):
  """Generates the header for API registration / declaration"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, _):  # namespace not relevant, this is a bundle
    c = code_util.Code()

    c.Append('#include <string>')
    c.Append()
    c.Append("class ExtensionFunctionRegistry;")
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Append('class %s {' %
             self._bundle._GenerateBundleClass('GeneratedFunctionRegistry'))
    c.Sblock(' public:')
    c.Append('static void RegisterAll('
             'ExtensionFunctionRegistry* registry);')
    c.Eblock('};')
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    return self._bundle._GenerateHeader('generated_api', c)


class _APICCGenerator(object):
  """Generates a code_util.Code object for the generated API .cc file"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, _):  # namespace not relevant, this is a bundle
    c = code_util.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append('#include "%s"' % (cpp_util.ToPosixPath(
        os.path.join(self._bundle._impl_dir, 'generated_api_registration.h'))))
    c.Append()
    c.Append('#include "build/build_config.h"')
    c.Append('#include "build/chromeos_buildflags.h"')
    c.Append()
    for namespace in self._bundle._model.namespaces.values():
      namespace_name = namespace.unix_name.replace("experimental_", "")
      implementation_header = namespace.compiler_options.get(
          "implemented_in", "%s/%s/%s_api.h" %
          (self._bundle._impl_dir, namespace_name, namespace_name))
      if not os.path.exists(
          os.path.join(self._bundle._root,
                       os.path.normpath(implementation_header))):
        if "implemented_in" in namespace.compiler_options:
          raise ValueError('Header file for namespace "%s" specified in '
                           'compiler_options not found: %s' %
                           (namespace.unix_name, implementation_header))
        continue
      ifdefs = self._bundle._GetPlatformIfdefs(namespace)
      if ifdefs is not None:
        c.Append("#if %s" % ifdefs, indent_level=0)

      c.Append('#include "%s"' % cpp_util.ToPosixPath(implementation_header))

      if ifdefs is not None:
        c.Append("#endif  // %s" % ifdefs, indent_level=0)
    c.Append()
    c.Append('#include '
             '"extensions/browser/extension_function_registry.h"')
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Concat(self._bundle._GenerateFunctionRegistryRegisterAll())
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    c.Append()
    return c


class _SchemasHGenerator(object):
  """Generates a code_util.Code object for the generated schemas .h file"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, _):  # namespace not relevant, this is a bundle
    c = code_util.Code()
    c.Append('#include <string_view>')
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Append('class %s {' %
             self._bundle._GenerateBundleClass('GeneratedSchemas'))
    c.Sblock(' public:')
    c.Append('// Determines if schema named |name| is generated.')
    c.Append('static bool IsGenerated(std::string_view name);')
    c.Append()
    c.Append('// Gets the API schema named |name|.')
    c.Append('static std::string_view Get(std::string_view name);')
    c.Eblock('};')
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    return self._bundle._GenerateHeader('generated_schemas', c)


def _FormatNameAsConstant(name):
  """Formats a name to be a C++ constant of the form kConstantName"""
  name = '%s%s' % (name[0].upper(), name[1:])
  return 'k%s' % re.sub('_[a-z]', lambda m: m.group(0)[1].upper(),
                        name.replace('.', '_'))


class _SchemasCCGenerator(object):
  """Generates a code_util.Code object for the generated schemas .cc file"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, _):  # namespace not relevant, this is a bundle
    c = code_util.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append('#include "%s"' % (cpp_util.ToPosixPath(
        os.path.join(self._bundle._source_file_dir, 'generated_schemas.h'))))
    c.Append()
    c.Append('#include <algorithm>')
    c.Append('#include <iterator>')
    c.Append('#include <string_view>')
    c.Append()
    c.Append('#include "base/containers/fixed_flat_map.h"')
    c.Append()
    c.Append('namespace {')
    for api in self._bundle._api_defs:
      namespace = self._bundle._model.namespaces[api.get('namespace')]
      json_content = json.dumps(_PrefixSchemaWithNamespace(
          _RemoveUnneededFields(api)),
                                separators=(',', ':'))
      # This will output a valid JSON C string. Note that some schemas are
      # too large to compile on windows. Split the JSON up into several
      # strings, since apparently that helps.
      max_length = 8192
      segments = [
          json_content[i:i + max_length]
          for i in range(0, len(json_content), max_length)
      ]
      c.Append(
          'constexpr char %s[] = R"R(%s)R";' %
          (_FormatNameAsConstant(namespace.name), ')R" R"R('.join(segments)))
    c.Append('}  // namespace')
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Append('// static')
    c.Sblock('bool %s::IsGenerated(std::string_view name) {' %
             self._bundle._GenerateBundleClass('GeneratedSchemas'))
    c.Append('return !Get(name).empty();')
    c.Eblock('}')
    c.Append()
    c.Append('// static')
    c.Sblock('std::string_view %s::Get(std::string_view name) {' %
             self._bundle._GenerateBundleClass('GeneratedSchemas'))

    c.Append('static constexpr auto kSchemas = '
             'base::MakeFixedFlatMap<std::string_view, std::string_view>({')
    c.Sblock()
    namespaces = [
        self._bundle._model.namespaces[api.get('namespace')].name
        for api in self._bundle._api_defs
    ]
    for namespace in sorted(namespaces):
      schema_constant_name = _FormatNameAsConstant(namespace)
      c.Append('{"%s", %s},' % (namespace, schema_constant_name))
    c.Eblock('});')
    c.Append('auto it = kSchemas.find(name);')
    c.Append('return it != kSchemas.end() ? it->second : std::string_view();')
    c.Eblock('}')
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    c.Append()
    return c
