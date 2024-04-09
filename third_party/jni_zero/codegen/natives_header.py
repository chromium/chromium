# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling C++ methods from Java."""

from codegen import convert_type
from codegen import header_common
import common


def _return_type_cpp(java_type):
  if converted_type := java_type.converted_type():
    return converted_type
  if java_type.is_primitive():
    return java_type.to_cpp()
  return f'jni_zero::ScopedJavaLocalRef<{java_type.to_cpp()}>'


def _param_type_cpp(java_type):
  if converted_type := java_type.converted_type():
    # Drop & when the type is obviously a pointer to avoid "const char *&".
    if not java_type.is_primitive() and not converted_type.endswith('*'):
      converted_type += '&'
    return converted_type

  ret = java_type.to_cpp()
  if java_type.is_primitive():
    return ret
  return f'const jni_zero::JavaParamRef<{ret}>&'


def _impl_forward_declaration(sb, native, params):
  sb('// Forward declaration. To be implemented by the including .cc file.\n')
  with sb.statement():
    sb(f'static {_return_type_cpp(native.return_type)} {native.cpp_impl_name}')
    with sb.param_list() as plist:
      plist.append('JNIEnv* env')
      if not native.static:
        plist.append('const jni_zero::JavaParamRef<jobject>& jcaller')
      plist.extend(f'{_param_type_cpp(p.java_type)} {p.name}' for p in params)


def _impl_call_params(sb, native, params):
  with sb.param_list() as plist:
    plist.append('env')
    if not native.static:
      plist.append(header_common.java_param_ref_expression(
          'jobject', 'jcaller'))
    for p in params:
      if p.java_type.converted_type():
        plist.append(f'{p.name}_converted')
      elif p.java_type.is_primitive():
        plist.append(p.name)
      else:
        c_type = p.java_type.to_cpp()
        plist.append(header_common.java_param_ref_expression(c_type, p.name))


def proxy_declaration(sb, jni_obj, native):
  stub_name = jni_obj.GetStubName(native)
  return_type_cpp = native.proxy_return_type.to_cpp()
  sb(f'JNI_BOUNDARY_EXPORT {return_type_cpp} {stub_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    jtype = 'jclass' if native.static else 'jobject'
    plist.append(f'{jtype} jcaller')
    plist.extend(f'{p.java_type.to_cpp()} {p.name}'
                 for p in native.proxy_params)


def _single_method(sb, jni_obj, native):
  cpp_class = native.first_param_cpp_type
  if cpp_class:
    params = native.proxy_params[1:]
  else:
    params = native.proxy_params
  if native.needs_implicit_array_element_class_param:
    params = params[:-1]

  # Only non-class methods need to be forward-declared.
  if not cpp_class:
    _impl_forward_declaration(sb, native, params)
    sb('\n')

  proxy_declaration(sb, jni_obj, native)
  with sb.block():
    if cpp_class:
      sb(f"""\
{cpp_class}* native = reinterpret_cast<{cpp_class}*>({native.params[0].name});
CHECK_NATIVE_PTR(env, jcaller, native, "{native.cpp_name}\"""")
      if default_value := native.return_type.to_cpp_default_value():
        sb(f', {default_value}')
      sb(')\n')

    for p in params:
      if p.java_type.converted_type():
        convert_type.from_jni_assignment(sb, f'{p.name}_converted', p.name,
                                         p.java_type)

    with sb.statement():
      if not native.return_type.is_void():
        sb('auto ret = ')
      if cpp_class:
        sb(f'native->{native.cpp_name}')
      else:
        sb(f'{native.cpp_impl_name}')
      _impl_call_params(sb, native, params)

    if not native.return_type.is_void():
      with sb.statement():
        sb('return ')
        if native.return_type.converted_type():
          if native.needs_implicit_array_element_class_param:
            clazz_param = native.proxy_params[-1]
          else:
            clazz_param = None
          convert_type.to_jni_expression(sb,
                                         'ret',
                                         native.return_type,
                                         clazz_param=clazz_param)
        else:
          sb('ret')

        if not native.return_type.is_primitive():
          sb('.Release()')


def methods(jni_obj):
  if not jni_obj.natives:
    return ''
  sb = common.StringBuilder()
  sb('// Java to native functions\n')
  for native in jni_obj.natives:
    _single_method(sb, jni_obj, native)
    sb('\n')
  return sb.to_string()
