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
    declarations.add(f'template<> {T} '
                     f'FromJniType<{T}>(JNIEnv*, const JavaRef<jobject>&);')

  for java_type in cpp_to_java_types:
    T = java_type.converted_type()
    declarations.add(f'template<> jni_zero::ScopedJavaLocalRef<jobject> '
                     f'ToJniType<{T}>(JNIEnv*, {T} const&);')

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
  """Writes a ToJniType() expression to |sb|.

  Args:
    rvalue: Snippet to use as input to ToJniType().
    java_type: Type containing the @JniType annotation.
    clazz_param: Snippet to use as the third parameter for array conversions.
  """
  T = java_type.converted_type()
  assert T
  if java_type.is_primitive():
    sb(f'static_cast<{java_type.to_cpp()}>({rvalue})')
    return

  if not java_type.is_array():
    sb(f'jni_zero::ToJniType<{T}>')
    sb.param_list(['env', rvalue])
    return

  element_type = java_type.to_array_element_type()
  if element_type.is_array():
    raise Exception(
        '@JniType() for multi-dimensional arrays are not yet supported. '
        'Found ' + T)
  sb(f'jni_zero::ConvertArray<{T}>::ToJniType')
  with sb.param_list() as plist:
    plist += ['env', rvalue]
    if not element_type.is_primitive():
      if clazz_param:
        plist += [clazz_param.name]
      else:
        plist += [
            header_common.class_accessor_expression(element_type.java_class)
        ]


def to_jni_assignment(sb, dest_var_name, src_var_name, java_type):
  """Writes a ToJniType() assignment to |sb|."""
  with sb.statement():
    if java_type.is_primitive():
      var_type = java_type.to_cpp()
    else:
      var_type = f'jni_zero::ScopedJavaLocalRef<jobject>'
    sb(f'{var_type} {dest_var_name} = ')
    to_jni_expression(sb, src_var_name, java_type)


def from_jni_expression(sb, rvalue, java_type, release_ref=False):
  """Writes a FromJniType() expression to |sb|.

  Args:
    rvalue: Snippet to use as input to FromJniType().
    java_type: Type containing the @JniType annotation.
    release_ref: Whether to release |rvalue| after conversion.
  """
  T = java_type.converted_type()
  assert T
  if java_type.is_primitive():
    sb(f'static_cast<{T}>({rvalue})')
    return

  if java_type.is_array():
    jtype = java_type.to_cpp()
    rvalue = f'static_cast<{jtype}>({rvalue})'
  else:
    jtype = 'jobject'

  if release_ref:
    rvalue = f'jni_zero::ScopedJavaLocalRef<{jtype}>(env, {rvalue})'
  else:
    rvalue = f'jni_zero::JavaParamRef<{jtype}>(env, {rvalue})'

  if not java_type.is_array():
    sb(f'jni_zero::FromJniType<{T}>')
    sb.param_list(['env', rvalue])
    return

  if java_type.array_dimensions > 1:
    raise Exception(
        '@JniType() for multi-dimensional arrays are not yet supported. '
        'Found ' + T)
  sb(f'jni_zero::ConvertArray<{T}>::FromJniType')
  sb.param_list(['env', rvalue])
