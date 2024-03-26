# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "intermediates" command."""

import base64
import collections
import dataclasses
import hashlib
import os
import pickle
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

from codegen import called_by_native_header
from codegen import convert_type
from codegen import header_common
from codegen import placeholder_gen_jni_java
from codegen import placeholder_java_type
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
    self.java_class = java_class
    self.name = parsed_method.name
    self.signature = parsed_method.signature
    self.is_proxy = is_proxy
    self.static = is_proxy or parsed_method.static
    self.native_class_name = parsed_method.native_class_name

    # Proxy methods don't have a native prefix so the first letter is
    # lowercase. But we still want the CPP declaration to use upper camel
    # case for the method name.
    self.cpp_name = common.capitalize(self.name)
    self.is_test_only = NameIsTestOnly(self.name)

    if self.is_proxy:
      self.needs_implicit_array_element_class_param = (
          proxy.needs_implicit_array_element_class_param(self.return_type))
      self.proxy_signature = self.signature.to_proxy()
      if self.needs_implicit_array_element_class_param:
        self.proxy_signature = proxy.add_implicit_array_element_class_param(
            self.proxy_signature)
      self.proxy_name, self.hashed_proxy_name = proxy.create_method_names(
          java_class, self.name, self.is_test_only)
    else:
      self.needs_implicit_array_element_class_param = False
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
  """Describes a Java method that is called from C++"""
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


def JavaTypeToCForDeclaration(java_type):
  """Wrap the C datatype in a JavaParamRef if required."""
  c_type = java_type.to_cpp()
  if java_type.is_primitive():
    return c_type
  return f'const jni_zero::JavaParamRef<{c_type}>&'


def JavaTypeToCForCalledByNativeParam(java_type):
  """Returns a C datatype to be when calling from native."""
  c_type = java_type.to_cpp()
  if converted_type := java_type.converted_type():
    if java_type.is_primitive():
      return converted_type
    return f'{converted_type} const&'
  if java_type.is_primitive():
    if c_type == 'jint':
      return 'JniIntWrapper'
    return c_type
  return f'const jni_zero::JavaRef<{c_type}>&'


def _GetJNIFirstParam(native, for_declaration):
  c_type = 'jclass' if native.static else 'jobject'

  if for_declaration:
    c_type = f'const jni_zero::JavaParamRef<{c_type}>&'
  return [c_type + ' jcaller']


def _GetParamsInDeclaration(native):
  """Returns the params for the forward declaration.

  Args:
    native: the native dictionary describing the method.

  Returns:
    A string containing the params.
  """
  ret = []
  for p in native.params:
    converted_type = p.java_type.converted_type()
    if converted_type:
      if not p.java_type.is_primitive():
        converted_type = f'{converted_type}&'
      ret.append(f'{converted_type} {p.name}')
    else:
      ret.append(JavaTypeToCForDeclaration(p.java_type) + ' ' + p.name)
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
  params = [p.java_type.to_cpp() + ' ' + p.name for p in native.proxy_params]
  params = _GetJNIFirstParam(native, False) + params
  return ',\n    '.join(params)


def NameIsTestOnly(name):
  return name.endswith(('ForTest', 'ForTests', 'ForTesting'))


def GetRegistrationFunctionName(fully_qualified_class):
  """Returns the register name with a given class."""
  return 'RegisterNative_' + common.escape_class_name(fully_qualified_class)


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


class JniObject:
  """Uses the given java source file to generate the JNI header file."""

  def __init__(self, parsed_file, options, *, from_javap):
    self.options = options
    self.filename = parsed_file.filename
    self.type_resolver = parsed_file.type_resolver
    self.module_name = parsed_file.module_name
    self.proxy_interface = parsed_file.proxy_interface
    self.proxy_visibility = parsed_file.proxy_visibility
    self.constant_fields = parsed_file.constant_fields
    # --per-file-natives is not available in all parsers.
    self.per_file_natives = getattr(options, 'per_file_natives', False)

    # These are different only for legacy reasons.
    if from_javap:
      self.jni_namespace = options.namespace or 'JNI_' + self.java_class.name
    else:
      self.jni_namespace = parsed_file.jni_namespace or options.namespace

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
          CalledByNative(parsed_called_by_native,
                         unchecked=from_javap and options.unchecked_exceptions,
                         is_system_class=from_javap))

    _AssignMethodIdFunctionNames(parsed_file.type_resolver, called_by_natives)
    self.called_by_natives = called_by_natives

    # from-jar does not define these flags.
    if natives:
      self.final_gen_jni_class = proxy.get_gen_jni_class(
          short=options.use_proxy_hash or options.enable_jni_multiplexing,
          name_prefix=self.module_name,
          package_prefix=options.package_prefix)
    else:
      self.final_gen_jni_class = None

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

  def GetStubName(self, native):
    """Return the name of the stub function for a native method."""
    if native.is_proxy:
      if self.options.use_proxy_hash:
        method_name = common.escape_class_name(native.hashed_proxy_name)
      else:
        method_name = common.escape_class_name(native.proxy_name)
      if self.per_file_natives:
        return 'Java_' + common.escape_class_name(
            f'{self.java_class.full_name_with_slashes}Jni/native{common.capitalize(native.name)}'
        )
      else:
        return 'Java_%s_%s' % (common.escape_class_name(
            self.final_gen_jni_class.full_name_with_slashes), method_name)

    escaped_name = common.escape_class_name(
        self.java_class.full_name_with_slashes)
    return f'Java_{escaped_name}_native{native.cpp_name}'


def _UsesConvertType(java_type):
  # Array conversions do not need to be declared and primitive conversions
  # are just static_cast.
  return bool(java_type.converted_type() and not java_type.is_array()
              and not java_type.is_primitive())


def _CollectConvertTypeTypes(natives, called_by_natives):
  java_to_cpp_types = []
  cpp_to_java_types = []

  for native in natives:
    java_to_cpp_types.extend(param.java_type for param in native.params
                             if _UsesConvertType(param.java_type))
    if _UsesConvertType(native.return_type):
      cpp_to_java_types.append(native.return_type)

  for called_by_native in called_by_natives:
    cpp_to_java_types.extend(param.java_type
                             for param in called_by_native.params
                             if _UsesConvertType(param.java_type))
    if _UsesConvertType(called_by_native.return_type):
      java_to_cpp_types.append(called_by_native.return_type)

  return java_to_cpp_types, cpp_to_java_types


def _CollectReferencedClasses(jni_obj):
  ret = set()
  # @CalledByNatives can appear on nested classes, so check each one.
  for called_by_native in jni_obj.called_by_natives:
    ret.add(called_by_native.java_class)
    for param in called_by_native.params:
      java_type = param.java_type
      if java_type.is_object_array() and java_type.converted_type():
        ret.add(java_type.java_class)


  # Find any classes needed for @JniType conversions.
  for native in jni_obj.proxy_natives:
    return_type = native.return_type
    if return_type.is_object_array() and return_type.converted_type():
      ret.add(return_type.java_class)
  return sorted(ret)


class InlHeaderFileGenerator:

  def __init__(self, jni_obj):
    self.jni_obj = jni_obj
    self.namespace = jni_obj.jni_namespace
    java_class = jni_obj.java_class
    self.java_class = java_class
    self.class_name = java_class.name
    self.module_name = jni_obj.module_name
    self.natives = jni_obj.natives
    self.called_by_natives = jni_obj.called_by_natives
    self.constant_fields = jni_obj.constant_fields
    self.type_resolver = jni_obj.type_resolver
    self.options = jni_obj.options

  def GetContent(self):
    """Returns the content of the JNI binding file."""
    template = Template("""\
${PREAMBLE}\
${CLASS_ACCESSORS}\
${CONVERSION_FUNCTION_DECLARATIONS}\
${CONSTANTS_ENUMS}\

// Method stubs.
$METHOD_STUBS
${EPILOGUE}
""")
    java_classes = _CollectReferencedClasses(self.jni_obj)
    preamble, epilogue = header_common.header_preamble(
        GetScriptName(),
        self.java_class,
        system_includes=['jni.h'],
        user_includes=['third_party/jni_zero/jni_export.h'] +
        self.options.extra_includes)
    class_accessors = header_common.class_accessors(java_classes,
                                                    self.jni_obj.module_name)
    java_to_cpp_types, cpp_to_java_types = _CollectConvertTypeTypes(
        self.natives, self.called_by_natives)
    conversion_declarations = convert_type.conversion_declarations(
        java_to_cpp_types, cpp_to_java_types)
    constants_enums = called_by_native_header.constants_enums(
        self.java_class, self.constant_fields)
    values = {
        'PREAMBLE': preamble,
        'EPILOGUE': epilogue,
        'CLASS_ACCESSORS': class_accessors,
        'CONVERSION_FUNCTION_DECLARATIONS': conversion_declarations,
        'CONSTANTS_ENUMS': constants_enums,
        'METHOD_STUBS': self.GetMethodStubsString(),
    }
    open_namespace = self.GetOpenNamespaceString()
    if open_namespace:
      close_namespace = self.GetCloseNamespaceString()
      values['METHOD_STUBS'] = '\n'.join(
          [open_namespace, values['METHOD_STUBS'], close_namespace])

      constants_enums = values['CONSTANTS_ENUMS']
      if constants_enums:
        values['CONSTANTS_ENUMS'] = '\n'.join(
            [open_namespace, constants_enums, close_namespace])

    return WrapOutput(template.substitute(values))

  def GetMethodStubsString(self):
    """Returns the code corresponding to method stubs."""
    ret = []
    for native in self.natives:
      ret += [self.GetNativeStub(native)]
    ret.append(called_by_native_header.method_stubs(self.called_by_natives))
    return '\n'.join(ret)

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

  def GetConvertedVarName(self, name):
    return name + '_converted'

  def GetImplementationMethodName(self, native):
    return 'JNI_%s_%s' % (self.java_class.name, native.cpp_name)

  def GetNativeStub(self, native):
    if native.first_param_cpp_type:
      params = native.proxy_params[1:]
    else:
      params = native.proxy_params
    if native.needs_implicit_array_element_class_param:
      params = params[:-1]

    params_in_call = ['env']
    if not native.static:
      # Add jcaller param.
      params_in_call.append(
          header_common.java_param_ref_expression('jobject', 'jcaller'))
    conversion_calls = []
    for p in params:
      if p.java_type.converted_type():
        name_in_call = self.GetConvertedVarName(p.name)
        params_in_call.append(name_in_call)
        conversion_calls.append(
            convert_type.from_jni_assignment(name_in_call, p.name, p.java_type))
      elif p.java_type.is_primitive():
        params_in_call.append(p.name)
      else:
        c_type = p.java_type.to_cpp()
        params_in_call.append(
            header_common.java_param_ref_expression(c_type, p.name))

    params_in_declaration = _GetParamsInDeclaration(native)
    params_in_call = ', '.join(params_in_call)

    return_type = native.return_type.to_cpp()
    return_declaration = return_type
    post_call = ''
    post_return_statement = ''
    return_phrase = 'return'
    if native.return_type.converted_type():
      return_phrase = 'auto ret ='
      clazz_param = None
      if native.needs_implicit_array_element_class_param:
        clazz_param = native.proxy_params[-1]
      convert_call = convert_type.to_jni_expression('ret',
                                                    native.return_type,
                                                    clazz_param=clazz_param)
      if not native.return_type.is_primitive():
        convert_call = f'{convert_call}.Release()'
      post_return_statement = f'return {convert_call};'
      return_declaration = native.return_type.converted_type()
    elif not native.return_type.is_primitive():
      post_call = '.Release()'
      return_declaration = ('jni_zero::ScopedJavaLocalRef<' + return_type + '>')

    values = {
        'RETURN': return_type,
        'RETURN_DECLARATION': return_declaration,
        'NAME': native.cpp_name,
        'IMPL_METHOD_NAME': self.GetImplementationMethodName(native),
        'PARAMS': ',\n    '.join(params_in_declaration),
        'PARAMS_IN_STUB': GetParamsInStub(native),
        'PARAMS_IN_CALL': params_in_call,
        'POST_CALL': post_call,
        'CONVERSION_CALLS': '\n  '.join(conversion_calls),
        'STUB_NAME': self.jni_obj.GetStubName(native),
        'POST_RETURN_STATEMENT': post_return_statement,
        'MAYBE_RETURN_PHRASE': return_phrase,
    }

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
  ${CONVERSION_CALLS}
  ${MAYBE_RETURN_PHRASE} native->${NAME}(${PARAMS_IN_CALL})${POST_CALL};
  ${POST_RETURN_STATEMENT}
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
  ${CONVERSION_CALLS}
  ${MAYBE_RETURN_PHRASE} ${IMPL_METHOD_NAME}(${PARAMS_IN_CALL})${POST_CALL};
  ${POST_RETURN_STATEMENT}
}
""")

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
  return '//third_party/jni_zero/jni_zero.py'


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
        ret.append(JniObject(parsed_file, args, from_javap=True))
  return ret


def _CreateSrcJar(srcjar_path,
                  gen_jni_class,
                  jni_objs,
                  *,
                  script_name,
                  per_file_natives=False):
  with common.atomic_output(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for jni_obj in jni_objs:
        if not jni_obj.proxy_natives:
          continue
        content = proxy_impl_java.Generate(jni_obj,
                                           gen_jni_class=gen_jni_class,
                                           script_name=script_name,
                                           per_file_natives=per_file_natives)
        zip_path = f'{jni_obj.java_class.class_without_prefix.full_name_with_slashes}Jni.java'
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)

      if not per_file_natives:
        content = placeholder_gen_jni_java.Generate(jni_objs,
                                                    gen_jni_class=gen_jni_class,
                                                    script_name=script_name)
        zip_path = f'{gen_jni_class.full_name_with_slashes}.java'
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)


def _CreatePlaceholderSrcJar(srcjar_path, jni_objs, *, script_name):
  with common.atomic_output(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for jni_obj in jni_objs:
        if not jni_obj.proxy_natives:
          continue
        main_class = jni_obj.type_resolver.java_class
        zip_path = main_class.class_without_prefix.full_name_with_slashes + '.java'
        content = placeholder_java_type.Generate(
            main_class,
            jni_obj.type_resolver.nested_classes,
            script_name=script_name,
            proxy_interface=jni_obj.proxy_interface,
            proxy_natives=jni_obj.proxy_natives)
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)
        placeholders = collections.defaultdict(list)
        for java_class in jni_obj.type_resolver.imports:
          # java.** are not separate deps and can be assumed to be available in
          # any compile and thus we do not need to create placeholders for them.
          if java_class.full_name_with_slashes.startswith('java/'):
            continue
          # TODO(mheikal): handle more than 1 nesting layer.
          if java_class.is_nested():
            placeholders[java_class.get_outer_class()].append(java_class)
          else:
            placeholders[java_class] = []
        for java_class, nested_classes in placeholders.items():
          zip_path = java_class.class_without_prefix.full_name_with_slashes + '.java'
          content = placeholder_java_type.Generate(java_class,
                                                   nested_classes,
                                                   script_name=script_name)
          common.add_to_zip_hermetic(srcjar, zip_path, data=content)


def _WriteHeaders(jni_objs, output_names, output_dir):
  for jni_obj, header_name in zip(jni_objs, output_names):
    output_file = os.path.join(output_dir, header_name)
    content = InlHeaderFileGenerator(jni_obj).GetContent()

    with common.atomic_output(output_file, 'w') as f:
      f.write(content)


def GenerateFromSource(parser, args):
  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  try:
    parsed_files = [
        parse.parse_java_file(f, package_prefix=args.package_prefix)
        for f in args.input_files
    ]
    jni_objs = [JniObject(x, args, from_javap=False) for x in parsed_files]
    _CheckNotEmpty(jni_objs)
    _CheckSameModule(jni_objs)
  except parse.ParseError as e:
    sys.stderr.write(f'{e}\n')
    sys.exit(1)

  _WriteHeaders(jni_objs, args.output_names, args.output_dir)

  jni_objs_with_proxy_natives = [x for x in jni_objs if x.proxy_natives]
  # Write .srcjar
  if args.srcjar_path:
    if jni_objs_with_proxy_natives:
      gen_jni_class = proxy.get_gen_jni_class(
          short=False,
          name_prefix=jni_objs_with_proxy_natives[0].module_name,
          package_prefix=args.package_prefix)
      _CreateSrcJar(args.srcjar_path,
                    gen_jni_class,
                    jni_objs_with_proxy_natives,
                    script_name=GetScriptName(),
                    per_file_natives=args.per_file_natives)
    else:
      # Only @CalledByNatives.
      zipfile.ZipFile(args.srcjar_path, 'w').close()
  if args.jni_pickle:
    with common.atomic_output(args.jni_pickle, 'wb') as f:
      pickle.dump(parsed_files, f)

  if args.placeholder_srcjar_path:
    if jni_objs_with_proxy_natives:
      _CreatePlaceholderSrcJar(args.placeholder_srcjar_path,
                               jni_objs_with_proxy_natives,
                               script_name=GetScriptName())
    else:
      zipfile.ZipFile(args.placeholder_srcjar_path, 'w').close()


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
