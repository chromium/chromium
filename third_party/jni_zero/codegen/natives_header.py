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
    name = f'JNI_{native.java_class.name}_{native.capitalized_name}'
    sb(f'static {_return_type_cpp(native.return_type)} {name}')
    with sb.param_list() as plist:
      plist.append('JNIEnv* env')
      if not native.static:
        plist.append('const jni_zero::JavaParamRef<jobject>& jcaller')
      plist.extend(f'{_param_type_cpp(p.java_type)} {p.cpp_name()}'
                   for p in params)


def _prep_param(sb, is_proxy, param):
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

  if is_proxy and java_type.to_cpp() != java_type.to_proxy().to_cpp():
    # E.g. jobject -> jstring
    orig_name = f'static_cast<{java_type.to_cpp()}>({orig_name})'
  return f'jni_zero::JavaParamRef<{java_type.to_cpp()}>(env, {orig_name})'


def entry_point_declaration(sb, jni_mode, jni_obj, native, gen_jni_class):
  """The method called by JNI, or by multiplexing methods."""
  if jni_mode.is_muxing and native.is_proxy:
    # In this case, it's not the symbol that JNI resolves, but the one the
    # switch table jumps to.
    function_name = native.muxed_entry_point_name
    define = 'JNI_ZERO_MUXED_ENTRYPOINT'
  else:
    function_name = native.boundary_name_cpp(jni_mode,
                                             gen_jni_class=gen_jni_class)
    define = 'JNI_ZERO_BOUNDARY_EXPORT'
  return_type_cpp = native.entry_point_return_type.to_cpp()
  params = native.entry_point_params(jni_mode)
  sb(f'{define} {return_type_cpp} {function_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not jni_mode.is_muxing:
      # The jclass param is never used, so do not bother adding it since muxed
      # entry points are not boundary (JNI) methods.
      jtype = 'jclass' if native.static else 'jobject'
      plist.append(f'{jtype} jcaller')
    plist.extend(f'{p.java_type.to_cpp()} {p.cpp_name()}' for p in params)


def entry_point_method(sb, jni_mode, jni_obj, native, gen_jni_class):
  """The method called by JNI, or by multiplexing methods."""
  params = native.params
  cpp_class = native.first_param_cpp_type
  if cpp_class:
    params = params[1:]

  # Only non-class methods need to be forward-declared.
  if not cpp_class:
    _impl_forward_declaration(sb, native, params)
    sb('\n')

  entry_point_declaration(sb, jni_mode, jni_obj, native, gen_jni_class)

  entry_point_return_type = native.entry_point_return_type
  return_type = native.return_type
  with sb.block(after='\n'):
    param_rvalues = [
        _prep_param(sb, native.is_proxy, param) for param in params
    ]

    with sb.statement():
      if not return_type.is_void():
        sb('auto _ret = ')
      if cpp_class:
        sb(f'reinterpret_cast<{cpp_class}*>({native.params[0].cpp_name()})'
           f'->{native.capitalized_name}')
      else:
        sb(f'JNI_{native.java_class.name}_{native.capitalized_name}')
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
        clazz_snippet = f'static_cast<jclass>({native.proxy_params[-1].name})'
      else:
        clazz_snippet = None
      convert_type.to_jni_expression(sb,
                                     '_ret',
                                     return_type,
                                     clazz_snippet=clazz_snippet)
      sb('.Release()')

    with sb.statement():
      sb('return ')
      if entry_point_return_type.to_cpp() != 'jobject':
        sb(f'static_cast<{entry_point_return_type.to_cpp()}>(converted_ret)')
      else:
        sb('converted_ret')


def multiplexing_boundary_method(sb, muxed_aliases, gen_jni_class):
  """The method called by JNI when multiplexing is enabled."""
  native = muxed_aliases[0]
  sig = native.muxed_signature
  has_switch_num = native.muxed_switch_num != -1
  boundary_name_cpp = native.boundary_name_cpp(common.JniMode.MUXING,
                                               gen_jni_class=gen_jni_class)
  sb(f'JNI_ZERO_BOUNDARY_EXPORT {sig.return_type.to_cpp()} {boundary_name_cpp}')
  param_names = []
  with sb.param_list() as plist:
    plist += ['JNIEnv* env', 'jclass jcaller']
    if has_switch_num:
      plist.append('jint switch_num')
    param_names += ['env']
    for i, p in enumerate(sig.param_list):
      plist.append(f'{p.java_type.to_cpp()} p{i}')
      param_names.append(f'p{i}')

  param_call_str = ', '.join(param_names)
  with sb.block():
    if not has_switch_num:
      sb(f'return {native.muxed_entry_point_name}({param_call_str});\n')
    else:
      num_aliases = len(muxed_aliases)
      sb(f'JNI_ZERO_DCHECK(switch_num >= 0 && switch_num < {num_aliases});\n')
      sb('switch (switch_num)')
      with sb.block():
        for native in muxed_aliases:
          sb(f'case {native.muxed_switch_num}:\n')
          sb(f'  return {native.muxed_entry_point_name}({param_call_str});\n')
        sb('default:\n')
        sb('  __builtin_unreachable();\n')
