# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "intermediates" command."""

import base64
import collections
import dataclasses
import hashlib
import os
import re
import shutil
from string import Template
import subprocess
import sys
import tempfile
import textwrap
import zipfile

_FILE_DIR = os.path.dirname(__file__)
_CHROMIUM_SRC = os.path.join(_FILE_DIR, os.pardir, os.pardir)
_BUILD_ANDROID_GYP = os.path.join(_CHROMIUM_SRC, 'build', 'android', 'gyp')

# Item 0 of sys.path is the directory of the main file; item 1 is PYTHONPATH
# (if set); item 2 is system libraries.
sys.path.insert(1, _BUILD_ANDROID_GYP)

from codegen import placeholder_gen_jni_java
from codegen import proxy_impl_java
import common
import java_types
import parse
import proxy

# Use 100 columns rather than 80 because it makes many lines more readable.
_WRAP_LINE_LENGTH = 100
# WrapOutput() is fairly slow. Pre-creating TextWrappers helps a bit.
_WRAPPERS_BY_INDENT = [
    textwrap.TextWrapper(width=_WRAP_LINE_LENGTH,
                         expand_tabs=False,
                         replace_whitespace=False,
                         subsequent_indent=' ' * (indent + 4),
                         break_long_words=False) for indent in range(50)
]  # 50 chosen experimentally.


class NativeMethod:
  """Describes a C/C++ method that is called by Java."""
  def __init__(self, parsed_method, *, java_class, is_proxy):
    self.name = parsed_method.name
    self.signature = parsed_method.signature
    self.is_proxy = is_proxy
    self.static = is_proxy or parsed_method.static
    self.native_class_name = parsed_method.native_class_name

    # Proxy methods don't have a native prefix so the first letter is
    # lowercase. But we still want the CPP declaration to use upper camel
    # case for the method name.
    self.cpp_name = common.capitalize(self.name)
    self.is_test_only = _NameIsTestOnly(self.name)

    if self.is_proxy:
      self.proxy_signature = self.signature.to_proxy()
      self.proxy_name, self.hashed_proxy_name = proxy.create_method_names(
          java_class, self.name, self.is_test_only)
      self.switch_num = None
    else:
      self.proxy_signature = self.signature

    first_param = self.params and self.params[0]
    if (first_param and first_param.java_type.is_primitive()
        and first_param.java_type.primitive_name == 'long'
        and first_param.name.startswith('native')):
      if parsed_method.native_class_name:
        self.first_param_cpp_type = parsed_method.native_class_name
      else:
        self.first_param_cpp_type = first_param.name[len('native'):]
    else:
      self.first_param_cpp_type = None

  @property
  def return_type(self):
    return self.signature.return_type

  @property
  def proxy_return_type(self):
    return self.proxy_signature.return_type

  @property
  def params(self):
    return self.signature.param_list

  @property
  def proxy_params(self):
    return self.proxy_signature.param_list


class CalledByNative:
  """Describes a java method exported to c/c++"""
  def __init__(self,
               parsed_called_by_native,
               *,
               is_system_class,
               unchecked=False):
    self.name = parsed_called_by_native.name
    self.signature = parsed_called_by_native.signature
    self.static = parsed_called_by_native.static
    self.unchecked = parsed_called_by_native.unchecked or unchecked
    self.java_class = parsed_called_by_native.java_class
    self.is_system_class = is_system_class

    # Computed once we know if overloads exist.
    self.method_id_function_name = None

  @property
  def is_constructor(self):
    return self.name == '<init>'

  @property
  def return_type(self):
    return self.signature.return_type

  @property
  def params(self):
    return self.signature.param_list

  @property
  def method_id_var_name(self):
    return f'{self.method_id_function_name}{len(self.params)}'


def JavaTypeToCForDeclaration(java_type):
  """Wrap the C datatype in a JavaParamRef if required."""
  c_type = java_type.to_cpp()
  if java_type.is_primitive():
    return c_type
  return f'const base::android::JavaParamRef<{c_type}>&'


def JavaTypeToCForCalledByNativeParam(java_type):
  """Returns a C datatype to be when calling from native."""
  c_type = java_type.to_cpp()
  if java_type.is_primitive():
    if c_type == 'jint':
      return 'JniIntWrapper'
    return c_type
  return f'const base::android::JavaRef<{c_type}>&'


def _GetJNIFirstParam(native, for_declaration):
  c_type = 'jclass' if native.static else 'jobject'

  if for_declaration:
    c_type = f'const base::android::JavaParamRef<{c_type}>&'
  return [c_type + ' jcaller']


def _GetParamsInDeclaration(native):
  """Returns the params for the forward declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  ret = [
      JavaTypeToCForDeclaration(p.java_type) + ' ' + p.name
      for p in native.params
  ]
  if not native.static:
    ret = _GetJNIFirstParam(native, True) + ret
  return ret


def GetParamsInStub(native):
  """Returns the params for the stub declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  params = [p.java_type.to_cpp() + ' ' + p.name for p in native.params]
  params = _GetJNIFirstParam(native, False) + params
  return ',\n    '.join(params)


def _NameIsTestOnly(name):
  return name.endswith(('ForTest', 'ForTests', 'ForTesting'))


def GetRegistrationFunctionName(fully_qualified_class):
  """Returns the register name with a given class."""
  return 'RegisterNative_' + common.escape_class_name(fully_qualified_class)


def _StaticCastForType(java_type):
  if java_type.is_primitive():
    return None
  ret = java_type.to_cpp()
  return None if ret == 'jobject' else ret


def _GetEnvCall(called_by_native):
  """Maps the types available via env->Call__Method."""
  if called_by_native.is_constructor:
    return 'NewObject'
  if called_by_native.return_type.is_primitive():
    name = called_by_native.return_type.primitive_name
    call = common.capitalize(called_by_native.return_type.primitive_name)
  else:
    call = 'Object'
  if called_by_native.static:
    call = 'Static' + call
  return 'Call' + call + 'Method'


def _MangleMethodName(type_resolver, name, param_types):
  mangled_types = []
  for java_type in param_types:
    if java_type.primitive_name:
      part = java_type.primitive_name
    else:
      part = type_resolver.contextualize(java_type.java_class).replace('.', '_')
    mangled_types.append(part + ('Array' * java_type.array_dimensions))

  return f'{name}__' + '__'.join(mangled_types)


def _AssignMethodIdFunctionNames(type_resolver, called_by_natives):
  # Mangle names for overloads with different number of parameters.
  def key(called_by_native):
    return (called_by_native.java_class.full_name_with_slashes,
            called_by_native.name, len(called_by_native.params))

  method_counts = collections.Counter(key(x) for x in called_by_natives)

  for called_by_native in called_by_natives:
    if called_by_native.is_constructor:
      method_id_function_name = 'Constructor'
    else:
      method_id_function_name = called_by_native.name

    if method_counts[key(called_by_native)] > 1:
      method_id_function_name = _MangleMethodName(
          type_resolver, method_id_function_name,
          called_by_native.signature.param_types)

    called_by_native.method_id_function_name = method_id_function_name


# Removes empty lines that are indented (i.e. start with 2x spaces).
def RemoveIndentedEmptyLines(string):
  return re.sub('^(?: {2})+$\n', '', string, flags=re.MULTILINE)


class JNIFromJavaP:
  """Uses 'javap' to parse a .class file and generate the JNI header file."""
  def __init__(self, parsed_file, options):
    self.options = options
    self.type_resolver = parsed_file.type_resolver

    called_by_natives = []
    for parsed_called_by_native in parsed_file.called_by_natives:
      called_by_natives.append(
          CalledByNative(parsed_called_by_native,
                         unchecked=options.unchecked_exceptions,
                         is_system_class=True))
    _AssignMethodIdFunctionNames(parsed_file.type_resolver, called_by_natives)
    self.called_by_natives = called_by_natives

    self.constant_fields = parsed_file.constant_fields
    self.jni_namespace = options.namespace or 'JNI_' + self.java_class.name

  def GetContent(self):
    # We pass in an empty string for the module (which will make the JNI use the
    # base module's files) for all javap-derived JNI. There may be a way to get
    # the module from a jar file, but it's not needed right now.
    generator = InlHeaderFileGenerator('', self.jni_namespace, self.java_class,
                                       [], self.called_by_natives,
                                       self.constant_fields, self.type_resolver,
                                       self.options)
    return generator.GetContent()

  @property
  def java_class(self):
    return self.type_resolver.java_class


class JNIFromJavaSource:
  """Uses the given java source file to generate the JNI header file."""
  def __init__(self, parsed_file, options):
    self.options = options
    self.filename = parsed_file.filename
    self.type_resolver = parsed_file.type_resolver
    self.jni_namespace = parsed_file.jni_namespace or options.namespace
    self.module_name = parsed_file.module_name
    self.proxy_interface = parsed_file.proxy_interface
    self.proxy_visibility = parsed_file.proxy_visibility

    natives = []
    for parsed_method in parsed_file.proxy_methods:
      natives.append(
          NativeMethod(parsed_method, java_class=self.java_class,
                       is_proxy=True))

    for parsed_method in parsed_file.non_proxy_methods:
      natives.append(
          NativeMethod(parsed_method,
                       java_class=self.java_class,
                       is_proxy=False))

    self.natives = natives

    called_by_natives = []
    for parsed_called_by_native in parsed_file.called_by_natives:
      called_by_natives.append(
          CalledByNative(parsed_called_by_native, is_system_class=False))

    _AssignMethodIdFunctionNames(parsed_file.type_resolver, called_by_natives)
    self.called_by_natives = called_by_natives

  @property
  def java_class(self):
    return self.type_resolver.java_class

  @property
  def proxy_natives(self):
    return [n for n in self.natives if n.is_proxy]

  @property
  def non_proxy_natives(self):
    return [n for n in self.natives if not n.is_proxy]

  def RemoveTestOnlyNatives(self):
    self.natives = [n for n in self.natives if not n.is_test_only]

  def GetContent(self):
    generator = InlHeaderFileGenerator(self.module_name, self.jni_namespace,
                                       self.java_class, self.natives,
                                       self.called_by_natives, [],
                                       self.type_resolver, self.options)
    return generator.GetContent()


class HeaderFileGeneratorHelper(object):
  """Include helper methods for header generators."""
  def __init__(self,
               java_class,
               *,
               module_name,
               package_prefix=None,
               split_name=None,
               use_proxy_hash=False,
               enable_jni_multiplexing=False):
    self.class_name = java_class.name
    self.module_name = module_name
    self.fully_qualified_class = java_class.full_name_with_slashes
    self.use_proxy_hash = use_proxy_hash
    self.package_prefix = package_prefix
    self.split_name = split_name
    self.enable_jni_multiplexing = enable_jni_multiplexing
    self.gen_jni_class = proxy.get_gen_jni_class(short=use_proxy_hash
                                                 or enable_jni_multiplexing,
                                                 name_prefix=module_name,
                                                 package_prefix=package_prefix)

  def GetStubName(self, native):
    """Return the name of the stub function for this native method.

    Args:
      native: the native dictionary describing the method.

    Returns:
      A string with the stub function name (used by the JVM).
    """
    if native.is_proxy:
      if self.use_proxy_hash:
        method_name = common.escape_class_name(native.hashed_proxy_name)
      else:
        method_name = common.escape_class_name(native.proxy_name)
      return 'Java_%s_%s' % (common.escape_class_name(
          self.gen_jni_class.full_name_with_slashes), method_name)

    template = Template('Java_${JAVA_NAME}_native${NAME}')

    java_name = self.fully_qualified_class

    values = {
        'NAME': native.cpp_name,
        'JAVA_NAME': common.escape_class_name(java_name)
    }
    return template.substitute(values)

  def GetUniqueClasses(self, origin):
    ret = collections.OrderedDict()
    for entry in origin:
      if isinstance(entry, NativeMethod) and entry.is_proxy:
        short_name = self.use_proxy_hash or self.enable_jni_multiplexing
        ret[self.gen_jni_class.name] = self.gen_jni_class.full_name_with_slashes
        continue
      ret[self.class_name] = self.fully_qualified_class

      if isinstance(entry, CalledByNative):
        class_name = entry.java_class.name
        jni_class_path = entry.java_class.full_name_with_slashes
      else:
        class_name = self.class_name
        jni_class_path = self.fully_qualified_class
      ret[class_name] = jni_class_path
    return ret

  def GetClassPathLines(self, classes, declare_only=False):
    """Returns the ClassPath constants."""
    ret = []
    if declare_only:
      template = Template("""
extern const char kClassPath_${JAVA_CLASS}[];
""")
    else:
      template = Template("""
JNI_ZERO_COMPONENT_BUILD_EXPORT extern const char kClassPath_${JAVA_CLASS}[];
const char kClassPath_${JAVA_CLASS}[] = \
"${JNI_CLASS_PATH}";
""")

    for full_clazz in classes.values():
      values = {
          'JAVA_CLASS': common.escape_class_name(full_clazz),
          'JNI_CLASS_PATH': full_clazz,
      }
      # Since all proxy methods use the same class, defining this in every
      # header file would result in duplicated extern initializations.
      if full_clazz != self.gen_jni_class.full_name_with_slashes:
        ret += [template.substitute(values)]

    class_getter = """\
#ifndef ${JAVA_CLASS}_clazz_defined
#define ${JAVA_CLASS}_clazz_defined
inline jclass ${JAVA_CLASS}_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_${JAVA_CLASS}, \
${MAYBE_SPLIT_NAME_ARG}&g_${JAVA_CLASS}_clazz);
}
#endif
"""
    if declare_only:
      template = Template("""\
extern std::atomic<jclass> g_${JAVA_CLASS}_clazz;
""" + class_getter)
    else:
      template = Template("""\
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_ZERO_COMPONENT_BUILD_EXPORT std::atomic<jclass> g_${JAVA_CLASS}_clazz(nullptr);
""" + class_getter)

    for full_clazz in classes.values():
      values = {
          'JAVA_CLASS':
          common.escape_class_name(full_clazz),
          'MAYBE_SPLIT_NAME_ARG':
          (('"%s", ' % self.split_name) if self.split_name else '')
      }
      # Since all proxy methods use the same class, defining this in every
      # header file would result in duplicated extern initializations.
      if full_clazz != self.gen_jni_class.full_name_with_slashes:
        ret += [template.substitute(values)]

    return ''.join(ret)


class InlHeaderFileGenerator(object):
  """Generates an inline header file for JNI integration."""
  def __init__(self, module_name, namespace, java_class, natives,
               called_by_natives, constant_fields, type_resolver, options):
    self.namespace = namespace
    self.java_class = java_class
    self.class_name = java_class.name
    self.natives = natives
    self.called_by_natives = called_by_natives
    self.header_guard = java_class.full_name_with_slashes.replace('/',
                                                                  '_') + '_JNI'
    self.constant_fields = constant_fields
    self.type_resolver = type_resolver
    self.options = options

    # from-jar does not define these flags.
    kwargs = {}
    if hasattr(options, 'use_proxy_hash'):
      kwargs['use_proxy_hash'] = options.use_proxy_hash
      kwargs['enable_jni_multiplexing'] = options.enable_jni_multiplexing
      kwargs['package_prefix'] = options.package_prefix

    self.helper = HeaderFileGeneratorHelper(java_class,
                                            module_name=module_name,
                                            split_name=options.split_name,
                                            **kwargs)

  def GetContent(self):
    """Returns the content of the JNI binding file."""
    template = Template("""\
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     ${SCRIPT_NAME}
// For
//     ${FULLY_QUALIFIED_CLASS}

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

#include "third_party/jni_zero/jni_export.h"
${INCLUDES}

// Step 1: Forward declarations.
$CLASS_PATH_DEFINITIONS

// Step 2: Constants (optional).

$CONSTANT_FIELDS\

// Step 3: Method stubs.
$METHOD_STUBS

#endif  // ${HEADER_GUARD}
""")
    values = {
        'SCRIPT_NAME': GetScriptName(),
        'FULLY_QUALIFIED_CLASS': self.java_class.full_name_with_slashes,
        'CLASS_PATH_DEFINITIONS': self.GetClassPathDefinitionsString(),
        'CONSTANT_FIELDS': self.GetConstantFieldsString(),
        'METHOD_STUBS': self.GetMethodStubsString(),
        'HEADER_GUARD': self.header_guard,
        'INCLUDES': self.GetIncludesString(),
    }
    open_namespace = self.GetOpenNamespaceString()
    if open_namespace:
      close_namespace = self.GetCloseNamespaceString()
      values['METHOD_STUBS'] = '\n'.join(
          [open_namespace, values['METHOD_STUBS'], close_namespace])

      constant_fields = values['CONSTANT_FIELDS']
      if constant_fields:
        values['CONSTANT_FIELDS'] = '\n'.join(
            [open_namespace, constant_fields, close_namespace])

    return WrapOutput(template.substitute(values))

  def GetClassPathDefinitionsString(self):
    classes = self.helper.GetUniqueClasses(self.called_by_natives)
    classes.update(self.helper.GetUniqueClasses(self.natives))
    return self.helper.GetClassPathLines(classes)

  def GetConstantFieldsString(self):
    if not self.constant_fields:
      return ''
    ret = ['enum Java_%s_constant_fields {' % self.java_class.name]
    for c in self.constant_fields:
      ret += ['  %s = %s,' % (c.name, c.value)]
    ret += ['};', '']
    return '\n'.join(ret)

  def GetMethodStubsString(self):
    """Returns the code corresponding to method stubs."""
    ret = []
    for native in self.natives:
      ret += [self.GetNativeStub(native)]
    ret += self.GetLazyCalledByNativeMethodStubs()
    return '\n'.join(ret)

  def GetLazyCalledByNativeMethodStubs(self):
    return [
        self.GetLazyCalledByNativeMethodStub(called_by_native)
        for called_by_native in self.called_by_natives
    ]

  def GetIncludesString(self):
    if not self.options.extra_includes:
      return ''
    includes = self.options.extra_includes
    return '\n'.join('#include "%s"' % x for x in includes) + '\n'

  def GetOpenNamespaceString(self):
    if self.namespace:
      all_namespaces = [
          'namespace %s {' % ns for ns in self.namespace.split('::')
      ]
      return '\n'.join(all_namespaces) + '\n'
    return ''

  def GetCloseNamespaceString(self):
    if self.namespace:
      all_namespaces = [
          '}  // namespace %s' % ns for ns in self.namespace.split('::')
      ]
      all_namespaces.reverse()
      return '\n' + '\n'.join(all_namespaces)
    return ''

  def GetCalledByNativeParamsInDeclaration(self, called_by_native):
    return ',\n    '.join([
        JavaTypeToCForCalledByNativeParam(p.java_type) + ' ' + p.name
        for p in called_by_native.params
    ])

  def GetJavaParamRefForCall(self, c_type, name):
    return Template(
        'base::android::JavaParamRef<${TYPE}>(env, ${NAME})').substitute({
            'TYPE':
            c_type,
            'NAME':
            name,
        })

  def GetImplementationMethodName(self, native):
    return 'JNI_%s_%s' % (self.java_class.name, native.cpp_name)

  def GetNativeStub(self, native):
    if native.first_param_cpp_type:
      params = native.params[1:]
    else:
      params = native.params

    params_in_call = ['env']
    if not native.static:
      # Add jcaller param.
      params_in_call.append(self.GetJavaParamRefForCall('jobject', 'jcaller'))

    for p in params:
      if p.java_type.is_primitive():
        params_in_call.append(p.name)
      else:
        c_type = p.java_type.to_cpp()
        params_in_call.append(self.GetJavaParamRefForCall(c_type, p.name))

    params_in_declaration = _GetParamsInDeclaration(native)
    params_in_call = ', '.join(params_in_call)

    return_type = native.return_type.to_cpp()
    return_declaration = return_type
    post_call = ''
    if not native.return_type.is_primitive():
      post_call = '.Release()'
      return_declaration = ('base::android::ScopedJavaLocalRef<' + return_type +
                            '>')

    values = {
        'RETURN': return_type,
        'RETURN_DECLARATION': return_declaration,
        'NAME': native.cpp_name,
        'IMPL_METHOD_NAME': self.GetImplementationMethodName(native),
        'PARAMS': ',\n    '.join(params_in_declaration),
        'PARAMS_IN_STUB': GetParamsInStub(native),
        'PARAMS_IN_CALL': params_in_call,
        'POST_CALL': post_call,
        'STUB_NAME': self.helper.GetStubName(native),
    }

    namespace_qual = self.namespace + '::' if self.namespace else ''
    if native.first_param_cpp_type:
      optional_error_return = native.return_type.to_cpp_default_value()
      if optional_error_return:
        optional_error_return = ', ' + optional_error_return
      values.update({
          'OPTIONAL_ERROR_RETURN': optional_error_return,
          'PARAM0_NAME': native.params[0].name,
          'P0_TYPE': native.first_param_cpp_type,
      })
      template = Template("""\
JNI_BOUNDARY_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB}) {
  ${P0_TYPE}* native = reinterpret_cast<${P0_TYPE}*>(${PARAM0_NAME});
  CHECK_NATIVE_PTR(env, jcaller, native, "${NAME}"${OPTIONAL_ERROR_RETURN});
  return native->${NAME}(${PARAMS_IN_CALL})${POST_CALL};
}
""")
    else:
      if values['PARAMS']:
        values['PARAMS'] = ', ' + values['PARAMS']
      template = Template("""\
static ${RETURN_DECLARATION} ${IMPL_METHOD_NAME}(JNIEnv* env${PARAMS});

JNI_BOUNDARY_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB}) {
  return ${IMPL_METHOD_NAME}(${PARAMS_IN_CALL})${POST_CALL};
}
""")

    return RemoveIndentedEmptyLines(template.substitute(values))

  def GetArgument(self, param):
    if param.java_type.is_primitive():
      if param.java_type.primitive_name == 'int':
        return f'as_jint({param.name})'
      return param.name
    return f'{param.name}.obj()'

  def GetCalledByNativeValues(self, called_by_native):
    """Fills in necessary values for the CalledByNative methods."""
    java_class_only = called_by_native.java_class.nested_name
    java_class = called_by_native.java_class.full_name_with_slashes

    if called_by_native.static or called_by_native.is_constructor:
      first_param_in_declaration = ''
      first_param_in_call = 'clazz'
    else:
      first_param_in_declaration = (
          ', const base::android::JavaRef<jobject>& obj')
      first_param_in_call = 'obj.obj()'
    params_in_declaration = self.GetCalledByNativeParamsInDeclaration(
        called_by_native)
    if params_in_declaration:
      params_in_declaration = ', ' + params_in_declaration
    params_in_call = ', '.join(
        self.GetArgument(p) for p in called_by_native.params)
    if params_in_call:
      params_in_call = ', ' + params_in_call
    check_exception = 'Unchecked'
    method_id_member_name = 'call_context.method_id'
    if not called_by_native.unchecked:
      check_exception = 'Checked'
      method_id_member_name = 'call_context.base.method_id'
    if called_by_native.is_constructor:
      return_type = called_by_native.java_class.as_type()
    else:
      return_type = called_by_native.return_type
    pre_call = ''
    post_call = ''
    static_cast = _StaticCastForType(return_type)
    if static_cast:
      pre_call = f'static_cast<{static_cast}>('
      post_call = ')'
    optional_error_return = return_type.to_cpp_default_value()
    if optional_error_return:
      optional_error_return = ', ' + optional_error_return
    return_declaration = ''
    return_clause = ''
    return_type_str = return_type.to_cpp()
    if not return_type.is_void():
      pre_call = ' ' + pre_call
      return_declaration = return_type_str + ' ret ='
      if return_type.is_primitive():
        return_clause = 'return ret;'
      else:
        return_type_str = (
            f'base::android::ScopedJavaLocalRef<{return_type_str}>')
        return_clause = f'return {return_type_str}(env, ret);'
    sig = called_by_native.signature
    jni_descriptor = sig.to_descriptor()

    return {
        'JAVA_CLASS_ONLY': java_class_only,
        'JAVA_CLASS': common.escape_class_name(java_class),
        'RETURN_TYPE': return_type_str,
        'OPTIONAL_ERROR_RETURN': optional_error_return,
        'RETURN_DECLARATION': return_declaration,
        'RETURN_CLAUSE': return_clause,
        'FIRST_PARAM_IN_DECLARATION': first_param_in_declaration,
        'PARAMS_IN_DECLARATION': params_in_declaration,
        'PRE_CALL': pre_call,
        'POST_CALL': post_call,
        'ENV_CALL': _GetEnvCall(called_by_native),
        'FIRST_PARAM_IN_CALL': first_param_in_call,
        'PARAMS_IN_CALL': params_in_call,
        'CHECK_EXCEPTION': check_exception,
        'JNI_NAME': called_by_native.name,
        'JNI_DESCRIPTOR': jni_descriptor,
        'METHOD_ID_MEMBER_NAME': method_id_member_name,
        'METHOD_ID_FUNCTION_NAME': called_by_native.method_id_function_name,
        'METHOD_ID_VAR_NAME': called_by_native.method_id_var_name,
        'METHOD_ID_TYPE': 'STATIC' if called_by_native.static else 'INSTANCE',
    }

  def GetLazyCalledByNativeMethodStub(self, called_by_native):
    """Returns a string."""
    function_signature_template = Template("""\
static ${RETURN_TYPE} Java_${JAVA_CLASS_ONLY}_${METHOD_ID_FUNCTION_NAME}(\
JNIEnv* env${FIRST_PARAM_IN_DECLARATION}${PARAMS_IN_DECLARATION})""")
    function_header_template = Template("""\
${FUNCTION_SIGNATURE} {""")
    function_header_with_unused_template = Template("""\
[[maybe_unused]] ${FUNCTION_SIGNATURE};
${FUNCTION_SIGNATURE} {""")
    template = Template("""
static std::atomic<jmethodID> g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME}(nullptr);
${FUNCTION_HEADER}
  jclass clazz = ${JAVA_CLASS}_clazz(env);
  CHECK_CLAZZ(env, ${FIRST_PARAM_IN_CALL},
      ${JAVA_CLASS}_clazz(env)${OPTIONAL_ERROR_RETURN});

  jni_generator::JniJavaCallContext${CHECK_EXCEPTION} call_context;
  call_context.Init<
      base::android::MethodID::TYPE_${METHOD_ID_TYPE}>(
          env,
          clazz,
          "${JNI_NAME}",
          "${JNI_DESCRIPTOR}",
          &g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME});

  ${RETURN_DECLARATION}
     ${PRE_CALL}env->${ENV_CALL}(${FIRST_PARAM_IN_CALL},
          ${METHOD_ID_MEMBER_NAME}${PARAMS_IN_CALL})${POST_CALL};
  ${RETURN_CLAUSE}
}""")
    values = self.GetCalledByNativeValues(called_by_native)
    values['FUNCTION_SIGNATURE'] = (
        function_signature_template.substitute(values))
    if called_by_native.is_system_class:
      values['FUNCTION_HEADER'] = (
          function_header_with_unused_template.substitute(values))
    else:
      values['FUNCTION_HEADER'] = function_header_template.substitute(values)
    return RemoveIndentedEmptyLines(template.substitute(values))

  def GetTraceEventForNameTemplate(self, name_template, values):
    name = Template(name_template).substitute(values)
    return '  TRACE_EVENT0("jni", "%s");\n' % name


def WrapOutput(output):
  ret = []
  for line in output.splitlines():
    # Do not wrap preprocessor directives or comments.
    if len(line) < _WRAP_LINE_LENGTH or line[0] == '#' or line.startswith('//'):
      ret.append(line)
    else:
      # Assumes that the line is not already indented as a continuation line,
      # which is not always true (oh well).
      first_line_indent = (len(line) - len(line.lstrip()))
      wrapper = _WRAPPERS_BY_INDENT[first_line_indent]
      ret.extend(wrapper.wrap(line))
  ret += ['']
  return '\n'.join(ret)


def GetScriptName():
  script_components = os.path.abspath(__file__).split(os.path.sep)
  base_index = 0
  for idx, value in enumerate(script_components):
    if value == 'base' or value == 'third_party':
      base_index = idx
      break
  return os.sep.join(script_components[base_index:])


def _RemoveStaleHeaders(path, output_names):
  if not os.path.isdir(path):
    return
  # Do not remove output files so that timestamps on declared outputs are not
  # modified unless their contents are changed (avoids reverse deps needing to
  # be rebuilt).
  preserve = set(output_names)
  for root, _, files in os.walk(path):
    for f in files:
      if f not in preserve:
        file_path = os.path.join(root, f)
        if os.path.isfile(file_path) and file_path.endswith('.h'):
          os.remove(file_path)


def _CheckSameModule(jni_objs):
  files_by_module = collections.defaultdict(list)
  for jni_obj in jni_objs:
    if jni_obj.proxy_natives:
      files_by_module[jni_obj.module_name].append(jni_obj.filename)
  if len(files_by_module) > 1:
    sys.stderr.write(
        'Multiple values for @NativeMethods(moduleName) is not supported.\n')
    for module_name, filenames in files_by_module.items():
      sys.stderr.write(f'module_name={module_name}\n')
      for filename in filenames:
        sys.stderr.write(f'  {filename}\n')
    sys.exit(1)


def _CheckNotEmpty(jni_objs):
  has_empty = False
  for jni_obj in jni_objs:
    if not (jni_obj.natives or jni_obj.called_by_natives):
      has_empty = True
      sys.stderr.write(f'No native methods found in {jni_obj.filename}.\n')
  if has_empty:
    sys.exit(1)


def _RunJavap(javap_path, class_file):
  p = subprocess.run([javap_path, '-s', '-constants', class_file],
                     text=True,
                     capture_output=True,
                     check=True)
  return p.stdout


def _ParseClassFiles(jar_file, class_files, args):
  # Parse javap output.
  ret = []
  with tempfile.TemporaryDirectory() as temp_dir:
    with zipfile.ZipFile(jar_file) as z:
      z.extractall(temp_dir, class_files)
      for class_file in class_files:
        class_file = os.path.join(temp_dir, class_file)
        contents = _RunJavap(args.javap, class_file)
        parsed_file = parse.parse_javap(class_file, contents)
        ret.append(JNIFromJavaP(parsed_file, args))
  return ret


def _CreateSrcJar(srcjar_path, gen_jni_class, jni_objs, *, script_name):
  with common.atomic_output(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for jni_obj in jni_objs:
        if not jni_obj.proxy_natives:
          continue
        content = proxy_impl_java.Generate(jni_obj,
                                           gen_jni_class=gen_jni_class,
                                           script_name=script_name)
        zip_path = f'{jni_obj.java_class.class_without_prefix.full_name_with_slashes}Jni.java'
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)

      content = placeholder_gen_jni_java.Generate(jni_objs,
                                                  gen_jni_class=gen_jni_class,
                                                  script_name=script_name)
      zip_path = f'{gen_jni_class.full_name_with_slashes}.java'
      common.add_to_zip_hermetic(srcjar, zip_path, data=content)


def _WriteHeaders(jni_objs, output_names, output_dir):
  for jni_obj, header_name in zip(jni_objs, output_names):
    output_file = os.path.join(output_dir, header_name)
    content = jni_obj.GetContent()
    with common.atomic_output(output_file, 'w') as f:
      f.write(content)


def _ParseSourceFiles(args):
  jni_objs = []
  for f in args.input_files:
    parsed_file = parse.parse_java_file(f, package_prefix=args.package_prefix)
    jni_objs.append(JNIFromJavaSource(parsed_file, args))
  return jni_objs


def GenerateFromSource(parser, args):
  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  try:
    jni_objs = _ParseSourceFiles(args)
    _CheckNotEmpty(jni_objs)
    _CheckSameModule(jni_objs)
  except parse.ParseError as e:
    sys.stderr.write(f'{e}\n')
    sys.exit(1)

  _WriteHeaders(jni_objs, args.output_names, args.output_dir)

  # Write .srcjar
  if args.srcjar_path:
    # module_name is set only for proxy_natives.
    jni_objs = [x for x in jni_objs if x.proxy_natives]
    if jni_objs:
      gen_jni_class = proxy.get_gen_jni_class(
          short=False,
          name_prefix=jni_objs[0].module_name,
          package_prefix=args.package_prefix)
      _CreateSrcJar(args.srcjar_path,
                    gen_jni_class,
                    jni_objs,
                    script_name=GetScriptName())
    else:
      # Only @CalledByNatives.
      zipfile.ZipFile(args.srcjar_path, 'w').close()


def GenerateFromJar(parser, args):
  if not args.javap:
    args.javap = shutil.which('javap')
    if not args.javap:
      parser.error('Could not find "javap" on your PATH. Use --javap to '
                   'specify its location.')

  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  try:
    jni_objs = _ParseClassFiles(args.jar_file, args.input_files, args)
  except parse.ParseError as e:
    sys.stderr.write(f'{e}\n')
    sys.exit(1)

  _WriteHeaders(jni_objs, args.output_names, args.output_dir)
