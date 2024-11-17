# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling C++ methods from Java."""

from codegen import convert_type
from codegen import header_common
import common


def _return_type_cpp(java_type):
  if converted_type := java_type.converted_type:
    return converted_type
  if java_type.is_primitive():
    return java_type.to_cpp()
  return f'jni_zero::ScopedJavaLocalRef<{java_type.to_cpp()}>'


def _param_type_cpp(java_type):
  if converted_type := java_type.converted_type:
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
      plist.extend(f'{_param_type_cpp(p.java_type)} {p.cpp_name()}'
                   for p in params)


def proxy_declaration(sb, jni_obj, native):
  stub_name = jni_obj.GetStubName(native)
  return_type_cpp = native.proxy_return_type.to_cpp()
  sb(f'JNI_POSSIBLE_BOUNDARY_EXPORT {return_type_cpp} {stub_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    jtype = 'jclass' if native.static else 'jobject'
    plist.append(f'{jtype} jcaller')
    plist.extend(f'{p.java_type.to_cpp()} {p.cpp_name()}'
                 for p in native.proxy_params)


def _prep_param(sb, param, proxy_type):
  """Returns the snippet to use for the parameter."""
  orig_name = param.cpp_name()
  java_type = param.java_type

  if java_type.converted_type:
    ret = f'{param.name}_converted'
    with sb.statement():
      sb(f'{java_type.converted_type} {ret} = ')
      convert_type.from_jni_expression(sb, orig_name, java_type)
    return ret

  if java_type.is_primitive():
    return orig_name

  if java_type.to_cpp() != proxy_type.to_cpp():
    # E.g. jobject -> jstring
    orig_name = f'static_cast<{java_type.to_cpp()}>({orig_name})'
  return f'jni_zero::JavaParamRef<{java_type.to_cpp()}>(env, {orig_name})'


def entry_point_method(sb, jni_obj, native):
  cpp_class = native.first_param_cpp_type
  if cpp_class:
    proxy_params = native.proxy_params[1:]
    params = native.params[1:]
  else:
    proxy_params = native.proxy_params
    params = native.params

  # Only non-class methods need to be forward-declared.
  if not cpp_class:
    _impl_forward_declaration(sb, native, params)
    sb('\n')

  proxy_declaration(sb, jni_obj, native)
  proxy_return_type = native.proxy_return_type
  return_type = native.return_type
  with sb.block(after='\n'):
    param_rvalues = [
        _prep_param(sb, param, proxy_param.java_type)
        for param, proxy_param in zip(params, proxy_params)
    ]

    with sb.statement():
      if not return_type.is_void():
        sb('auto _ret = ')
      if cpp_class:
        sb(f'reinterpret_cast<{cpp_class}*>({native.params[0].cpp_name()})'
           f'->{native.cpp_name}')
      else:
        sb(f'{native.cpp_impl_name}')
      with sb.param_list() as plist:
        plist.append('env')
        if not native.static:
          plist.append('jni_zero::JavaParamRef<jobject>(env, jcaller)')
        plist.extend(param_rvalues)

    if return_type.is_void():
      return

    if not return_type.converted_type:
      if return_type.is_primitive():
        sb('return _ret;\n')
      else:
        # Use ReleaseLocal() to ensure we are not calling .Release() on a
        # global ref. https://crbug.com/40944912
        sb('return _ret.ReleaseLocal();\n')
      return

    with sb.statement():
      sb('jobject converted_ret = ')
      if native.needs_implicit_array_element_class_param:
        clazz_param = proxy_params[-1]
      else:
        clazz_param = None
      convert_type.to_jni_expression(sb,
                                     '_ret',
                                     return_type,
                                     clazz_param=clazz_param)
      sb('.Release()')

    with sb.statement():
      sb('return ')
      if proxy_return_type.to_cpp() != 'jobject':
        sb(f'static_cast<{proxy_return_type.to_cpp()}>(converted_ret)')
      else:
        sb('converted_ret')
