# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "from-source" and "from-jar" commands."""

import collections
import os
import pickle
import shutil
import subprocess
import sys
import tempfile
import zipfile

from codegen import called_by_native_header
from codegen import convert_type
from codegen import header_common
from codegen import natives_header
from codegen import placeholder_gen_jni_java
from codegen import placeholder_java_type
from codegen import proxy_impl_java
import common
import java_types
import parse
import proxy


class NativeMethod:
  """Describes a C/C++ method that is called by Java."""
  def __init__(self, parsed_method, *, java_class, is_proxy):
    # The Java class the containing the natives. Never a nested class.
    self.java_class = java_class
    self.is_proxy = is_proxy
    # The method name. For non-proxy natives, this omits the "native" prefix.
    self.name = parsed_method.name
    self.capitalized_name = common.capitalize(self.name)
    self.is_test_only = NameIsTestOnly(parsed_method.name)
    self.signature = parsed_method.signature
    self.static = self.is_proxy or parsed_method.static
    # Value of @NativeClassQualifiedName.
    self.native_class_name = parsed_method.native_class_name

    # True when an extra jclass parameter should be added.
    self.needs_implicit_array_element_class_param = (
        self.is_proxy
        and proxy.needs_implicit_array_element_class_param(self.return_type))

    if self.is_proxy:
      # Signature with all reference types changed to "Object".
      self.proxy_signature = self.signature.to_proxy()
      if self.needs_implicit_array_element_class_param:
        self.proxy_signature = proxy.add_implicit_array_element_class_param(
            self.proxy_signature)
      # proxy_signature with params reordered. Does not include switch_num.
      self.muxed_signature = proxy.muxed_signature(self.proxy_signature)

      # Name to use when using per-file natives.
      # "native" prefix to not conflict with interface method names.
      self.per_file_name = f'native{self.capitalized_name}'
      # Method name within the GEN_JNI class.
      self.proxy_name = f'{java_class.to_cpp()}_{self.name}'
      # Method name within the J class (when is_hashing=True).
      # TODO(agrieve): No need to mangle before hashing.
      self.hashed_name = proxy.hashed_name(
          common.jni_mangle(f'{java_class.full_name_with_slashes}/{self.name}'),
          self.is_test_only)
      # Method name within the J class (when is_muxing=True).
      self.muxed_name = proxy.muxed_name(self.muxed_signature)
      # Name of C++ function that will be called from switch tables.
      self.muxed_entry_point_name = f'Muxed_{self.proxy_name}'
      # Switch statement index when multiplexing.
      self.muxed_switch_num = None

    # Set when the first param dictates this is implemented as a member
    # function of the native class given as the first parameter.
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
  def params(self):
    return self.signature.param_list

  @property
  def return_type(self):
    return self.signature.return_type

  @property
  def proxy_params(self):
    return self.proxy_signature.param_list

  @property
  def proxy_return_type(self):
    return self.proxy_signature.return_type

  @property
  def muxed_params(self):
    return self.muxed_signature.param_list

  @property
  def entry_point_return_type(self):
    return self.proxy_return_type if self.is_proxy else self.return_type

  def entry_point_params(self, jni_mode):
    """Params to use for entry point functions."""
    if not self.is_proxy:
      return self.params
    if jni_mode.is_muxing:
      return self.muxed_params
    return self.proxy_params

  def boundary_name(self, jni_mode):
    """Java name of the JNI native method."""
    if not self.is_proxy:
      return f'native{self.name}'
    if jni_mode.is_per_file:
      return f'native{self.capitalized_name}'
    if jni_mode.is_muxing:
      return self.muxed_name
    if jni_mode.is_hashing:
      return self.hashed_name
    return self.proxy_name

  def boundary_name_cpp(self, jni_mode, gen_jni_class=None):
    """C++ name of the JNI native method."""
    if not self.is_proxy:
      mangled_class_name = self.java_class.to_cpp()
    elif jni_mode.is_per_file:
      mangled_class_name = self.java_class.to_cpp() + 'Jni'
    else:
      mangled_class_name = gen_jni_class.to_cpp()

    method_name = self.boundary_name(jni_mode=jni_mode)
    mangled_method_name = common.jni_mangle(method_name)
    return f'Java_{mangled_class_name}_{mangled_method_name}'


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


def NameIsTestOnly(name):
  return name.endswith(('ForTest', 'ForTests', 'ForTesting'))


def _MangleMethodName(type_resolver, name, param_types):
  # E.g. java.util.List.reversed() has overloads that return different types.
  if not param_types:
    return name
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
  cbn_by_name = collections.defaultdict(list)

  for called_by_native in called_by_natives:
    if called_by_native.is_constructor:
      method_id_function_name = 'Constructor'
    else:
      method_id_function_name = called_by_native.name

    if method_counts[key(called_by_native)] > 1:
      method_id_function_name = _MangleMethodName(
          type_resolver, method_id_function_name,
          called_by_native.signature.param_types)
      cbn_by_name[method_id_function_name].append(called_by_native)

    called_by_native.method_id_function_name = method_id_function_name

  # E.g. java.util.List.reversed() has overloads that return different types.
  for duplicates in cbn_by_name.values():
    for i, cbn in enumerate(duplicates[1:], 1):
      cbn.method_id_function_name += str(i)


class JniObject:
  """Uses the given java source file to generate the JNI header file."""

  def __init__(self,
               parsed_file,
               *,
               from_javap,
               default_namespace=None,
               javap_unchecked_exceptions=False):
    self.filename = parsed_file.filename
    self.type_resolver = parsed_file.type_resolver
    self.module_name = parsed_file.module_name
    self.proxy_interface = parsed_file.proxy_interface
    self.proxy_visibility = parsed_file.proxy_visibility
    self.constant_fields = parsed_file.constant_fields

    # These are different only for legacy reasons.
    if from_javap:
      self.jni_namespace = default_namespace or 'JNI_' + self.java_class.name.replace(
          '$', '__')
    else:
      self.jni_namespace = parsed_file.jni_namespace or default_namespace

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
                         unchecked=from_javap and javap_unchecked_exceptions,
                         is_system_class=from_javap))

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

  def GetClassesToBeImported(self):
    classes = set()
    for n in self.proxy_natives:
      for t in list(n.signature.param_types) + [n.return_type]:
        class_obj = t.java_class
        if class_obj is None:
          # Primitive types will be None.
          continue
        if class_obj.full_name_with_slashes.startswith('java/lang/'):
          # java.lang** are never imported.
          continue
        classes.add(class_obj)

    return sorted(classes)

  def RemoveTestOnlyNatives(self):
    self.natives = [n for n in self.natives if not n.is_test_only]


def _CollectReferencedClasses(jni_obj):
  ret = set()
  # @CalledByNatives can appear on nested classes, so check each one.
  for called_by_native in jni_obj.called_by_natives:
    ret.add(called_by_native.java_class)
    for param in called_by_native.params:
      java_type = param.java_type
      if java_type.is_object_array() and java_type.converted_type:
        ret.add(java_type.java_class)


  # Find any classes needed for @JniType conversions.
  for native in jni_obj.proxy_natives:
    return_type = native.return_type
    if return_type.is_object_array() and return_type.converted_type:
      ret.add(return_type.java_class)
  return sorted(ret)


def _generate_header(jni_mode, jni_obj, extra_includes, gen_jni_class):
  preamble, epilogue = header_common.header_preamble(
      GetScriptName(),
      jni_obj.java_class,
      system_includes=['jni.h'],
      user_includes=['third_party/jni_zero/jni_export.h'] + extra_includes)
  java_classes = _CollectReferencedClasses(jni_obj)
  sb = common.StringBuilder()
  sb(preamble)

  if java_classes:
    with sb.section('Class Accessors'):
      header_common.class_accessors(sb, java_classes, jni_obj.module_name)

  with sb.namespace(jni_obj.jni_namespace):
    if jni_obj.constant_fields:
      with sb.section('Constants'):
        called_by_native_header.constants_enums(sb, jni_obj.java_class,
                                                jni_obj.constant_fields)

    if jni_obj.natives:
      with sb.section('Java to native functions'):
        for native in jni_obj.natives:
          natives_header.entry_point_method(sb, jni_mode, jni_obj, native,
                                            gen_jni_class)

    if jni_obj.called_by_natives:
      with sb.section('Native to Java functions'):
        for called_by_native in jni_obj.called_by_natives:
          called_by_native_header.method_definition(sb, called_by_native)

  sb(epilogue)
  return sb.to_string()


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
  return next(iter(files_by_module)) if files_by_module else None


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
        ret.append(
            JniObject(parsed_file,
                      from_javap=True,
                      default_namespace=args.namespace,
                      javap_unchecked_exceptions=args.unchecked_exceptions))
  return ret


def _CreateSrcJar(srcjar_path, jni_mode, gen_jni_class, jni_objs, *,
                  script_name):
  with common.atomic_output(srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      for jni_obj in jni_objs:
        if not jni_obj.proxy_natives:
          continue
        content = proxy_impl_java.Generate(jni_mode,
                                           jni_obj,
                                           gen_jni_class=gen_jni_class,
                                           script_name=script_name)
        zip_path = f'{jni_obj.java_class.class_without_prefix.full_name_with_slashes}Jni.java'
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)

      if not jni_mode.is_per_file:
        content = placeholder_gen_jni_java.Generate(jni_objs,
                                                    gen_jni_class=gen_jni_class,
                                                    script_name=script_name)
        zip_path = f'{gen_jni_class.full_name_with_slashes}.java'
        common.add_to_zip_hermetic(srcjar, zip_path, data=content)


def _CreatePlaceholderSrcJar(srcjar_path, jni_objs, *, script_name):
  already_added = set()
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
        already_added.add(zip_path)
        # In rare circumstances, another file in our generate_jni list will
        # import the FooJni from another class within the same generate_jni
        # target. We want to make sure we don't make placeholders for these, but
        # we do want placeholders for all BarJni classes that aren't a part of
        # this generate_jni.
        fake_zip_path = main_class.class_without_prefix.full_name_with_slashes + 'Jni.java'
        already_added.add(fake_zip_path)

      placeholders = collections.defaultdict(list)
      # Doing this in 2 phases to ensure that the Jni classes (the ones that
      # can have @NativeMethods) all get added first, so we don't accidentally
      # write a stubbed version of the class if it's imported by another class.
      for jni_obj in jni_objs:
        for java_class in jni_obj.GetClassesToBeImported():
          if java_class.full_name_with_slashes.startswith('java/'):
            continue
          # TODO(mheikal): handle more than 1 nesting layer.
          if java_class.is_nested():
            placeholders[java_class.get_outer_class()].append(java_class)
          elif java_class not in placeholders:
            placeholders[java_class] = []
      for java_class, nested_classes in placeholders.items():
        zip_path = java_class.class_without_prefix.full_name_with_slashes + '.java'
        if zip_path not in already_added:
          content = placeholder_java_type.Generate(java_class,
                                                   nested_classes,
                                                   script_name=script_name)
          common.add_to_zip_hermetic(srcjar, zip_path, data=content)
          already_added.add(zip_path)


def _WriteHeaders(jni_mode,
                  jni_objs,
                  output_names,
                  output_dir,
                  extra_includes,
                  gen_jni_class=None):
  for jni_obj, header_name in zip(jni_objs, output_names):
    output_file = os.path.join(output_dir, header_name)
    content = _generate_header(jni_mode, jni_obj, extra_includes, gen_jni_class)

    with common.atomic_output(output_file, 'w') as f:
      f.write(content)


def GenerateFromSource(parser, args, jni_mode):
  # Remove existing headers so that moving .java source files but not updating
  # the corresponding C++ include will be a compile failure (otherwise
  # incremental builds will usually not catch this).
  _RemoveStaleHeaders(args.output_dir, args.output_names)

  try:
    parsed_files = [
        parse.parse_java_file(f,
                              package_prefix=args.package_prefix,
                              package_prefix_filter=args.package_prefix_filter)
        for f in args.input_files
    ]
    jni_objs = [
        JniObject(x, from_javap=False, default_namespace=args.namespace)
        for x in parsed_files
    ]
    _CheckNotEmpty(jni_objs)
    module_name = _CheckSameModule(jni_objs)
  except parse.ParseError as e:
    sys.stderr.write(f'{e}\n')
    sys.exit(1)

  gen_jni_class = proxy.get_gen_jni_class(
      short=jni_mode.is_hashing or jni_mode.is_muxing,
      name_prefix=args.module_name or module_name,
      package_prefix=args.package_prefix,
      package_prefix_filter=args.package_prefix_filter)

  _WriteHeaders(jni_mode, jni_objs, args.output_names, args.output_dir,
                args.extra_includes, gen_jni_class)

  jni_objs_with_proxy_natives = [x for x in jni_objs if x.proxy_natives]
  # Write .srcjar
  if args.srcjar_path:
    if jni_objs_with_proxy_natives:
      gen_jni_class = proxy.get_gen_jni_class(
          short=False,
          name_prefix=jni_objs_with_proxy_natives[0].module_name,
          package_prefix=args.package_prefix,
          package_prefix_filter=args.package_prefix_filter)
      _CreateSrcJar(args.srcjar_path,
                    jni_mode,
                    gen_jni_class,
                    jni_objs_with_proxy_natives,
                    script_name=GetScriptName())
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


def GenerateFromJar(parser, args, jni_mode):
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

  _WriteHeaders(jni_mode, jni_objs, args.output_names, args.output_dir,
                args.extra_includes)
