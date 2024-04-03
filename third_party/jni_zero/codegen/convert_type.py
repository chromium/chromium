# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen related to @JniType."""

from codegen import header_common
import java_types


def conversion_declarations(java_to_cpp_types, cpp_to_java_types):
  declarations = set()
  for java_type in java_to_cpp_types:
    T = java_type.converted_type()
    J = java_type.to_cpp()
    declarations.add(f'template<> {T} '
                     f'FromJniType<{T}, {J}>(JNIEnv*, const JavaRef<{J}>&);')

  for java_type in cpp_to_java_types:
    T = java_type.converted_type()
    J = java_type.to_cpp()
    declarations.add(f'template<> jni_zero::ScopedJavaLocalRef<{J}> '
                     f'ToJniType<{T}, {J}>(JNIEnv*, {T} const&);')

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


def to_jni_expression(sb, rvalue, java_type, clazz_param=None):
  """Returns a conversion call expression from specified @JniType to default jni type."""
  if java_type.is_primitive():
    if java_type.primitive_name == 'int' and not java_type.converted_type():
      rvalue = f'as_jint({rvalue})'
    sb(f'static_cast<{java_type.to_cpp()}>({rvalue})')
    return

  T = java_type.converted_type()
  if not java_type.is_array():
    sb(f'jni_zero::ToJniType<{T}, {java_type.to_cpp()}>')
    sb.param_list(['env', rvalue])
    return

  element_type = java_type.to_array_element_type()
  if element_type.is_array():
    raise Exception(
        '@JniType() for multi-dimensional arrays are not yet supported. '
        'Found ' + T)
  if element_type.is_primitive():
    sb(f'jni_zero::ConvertArray<{T}>::ToJniType')
    sb.param_list(['env', rvalue])
    return

  if clazz_param:
    clazz_expr = clazz_param.name
  else:
    clazz_expr = header_common.class_accessor_expression(
        element_type.java_class)
  J = element_type.to_cpp()
  sb(f'jni_zero::ConvertArray<{T}>::ToJniType<{J}>')
  sb.param_list(['env', rvalue, clazz_expr])


def to_jni_assignment(sb, dest_var_name, src_var_name, java_type):
  """Returns a conversion statement from specified @JniType to default jni type."""
  with sb.statement():
    if java_type.is_primitive():
      var_type = java_type.to_cpp()
    else:
      var_type = f'jni_zero::ScopedJavaLocalRef<{java_type.to_cpp()}>'
    sb(f'{var_type} {dest_var_name} = ')
    to_jni_expression(sb, src_var_name, java_type)


def from_jni_expression(sb, rvalue, java_type):
  """Returns a conversion call expression from default jni type to specified @JniType."""
  T = java_type.converted_type()
  J = java_type.to_cpp()
  if java_type.is_primitive():
    sb(f'static_cast<{T}>({rvalue})')
    return

  if not java_type.is_primitive():
    rvalue = header_common.java_param_ref_expression(J, rvalue)

  if not java_type.is_array():
    sb(f'jni_zero::FromJniType<{T}, {J}>')
    sb.param_list(['env', rvalue])
    return

  element_type = java_type.to_array_element_type()
  if element_type.is_array():
    raise Exception(
        '@JniType() for multi-dimensional arrays are not yet supported. '
        'Found ' + T)
  if element_type.is_primitive():
    sb(f'jni_zero::ConvertArray<{T}>::FromJniType')
    sb.param_list(['env', rvalue])
    return

  J = java_type.to_array_element_type().to_cpp()
  sb(f'jni_zero::ConvertArray<{T}>::FromJniType<{J}>')
  sb.param_list(['env', rvalue])


def from_jni_assignment(sb, dst_var_name, src_var_name, java_type):
  """Returns a conversion statement from default jni type to specified @JniType."""
  with sb.statement():
    sb(f'{java_type.converted_type()} {dst_var_name} = ')
    from_jni_expression(sb, src_var_name, java_type)
