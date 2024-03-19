# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen related to @JniType."""

from codegen import header_common
import java_types


def conversion_declarations(java_to_cpp_types, cpp_to_java_types):
  declarations = set()
  for java_type in java_to_cpp_types:
    c = java_type.converted_type()
    j = f'const JavaRef<{java_type.to_cpp()}>&'
    declarations.add(f'template<> {c} FromJniType<{c}>(JNIEnv*, {j});')

  for java_type in cpp_to_java_types:
    c = java_type.converted_type()
    j = f'jni_zero::ScopedJavaLocalRef<{java_type.to_cpp()}>'
    declarations.add(f'template<> {j} ToJniType<{c}>(JNIEnv*, const {c}&);')

  if not declarations:
    return ''

  declaration_lines = '\n'.join(sorted(declarations))
  return f"""\
// Forward declare used conversion functions to avoid a compiler warning that
// triggers if a conversion specialization exists within the including .cc file.
namespace jni_zero {{
{declaration_lines}
}}  // namespace jni_zero
"""


def to_jni_expression(rvalue, java_type, clazz_param=None):
  """Returns a conversion call expression from specified @JniType to default jni type."""
  if java_type.is_primitive():
    if java_type.primitive_name == 'int':
      rvalue = f'as_jint({rvalue})'
    return f'static_cast<{java_type.to_cpp()}>({rvalue})'

  converted_type = java_type.converted_type()

  if java_type.is_array():
    if java_type.is_primitive_array():
      maybe_java_clazz = ''
    elif clazz_param:
      maybe_java_clazz = ', ' + clazz_param.name
    else:
      maybe_java_clazz = ', ' + header_common.class_accessor_expression(
          java_type.java_class)
    return (f'jni_zero::ConvertArray<{converted_type}>::ToJniType(env, '
            f'{rvalue}{maybe_java_clazz})')

  return f'jni_zero::ToJniType<{converted_type}>(env, {rvalue})'


def to_jni_assignment(dest_var_name, src_var_name, java_type):
  """Returns a conversion statement from specified @JniType to default jni type."""
  if java_type.is_primitive():
    var_type = java_type.to_cpp()
  else:
    var_type = f'jni_zero::ScopedJavaLocalRef<{java_type.to_cpp()}>'
  expr = to_jni_expression(src_var_name, java_type)
  return f'{var_type} {dest_var_name} = {expr};'


def from_jni_expression(rvalue, java_type):
  """Returns a conversion call expression from default jni type to specified @JniType."""
  converted_type = java_type.converted_type()
  if java_type.is_primitive():
    return f'static_cast<{converted_type}>({rvalue})'

  original_type = java_type.to_cpp()
  if not java_type.is_primitive():
    rvalue = header_common.java_param_ref_expression(original_type, rvalue)

  if java_type.is_array():
    if java_type.java_class == java_types.STRING_CLASS:
      template_arg = '<jstring>'
    else:
      template_arg = ''
    return (f'jni_zero::ConvertArray<{converted_type}>::FromJniType'
            f'{template_arg}(env, {rvalue})')

  return f'jni_zero::FromJniType<{converted_type}>(env, {rvalue})'


def from_jni_assignment(dst_var_name, src_var_name, java_type):
  """Returns a conversion statement from default jni type to specified @JniType."""
  var_type = java_type.converted_type()
  expr = from_jni_expression(src_var_name, java_type)
  return f'{var_type} {dst_var_name} = {expr};'
