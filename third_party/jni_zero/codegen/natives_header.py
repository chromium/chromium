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


def _param_type_cpp_non_mirror(java_type, use_const=False):
  if converted_type := java_type.converted_type:
    # Drop & when the type is obviously a pointer to avoid "const char *&".
    if not java_type.is_primitive() and not converted_type.endswith('*'):
      converted_type += '&'
      if use_const and not converted_type.startswith('const '):
        converted_type = 'const ' + converted_type
    return converted_type

  ret = java_type.to_cpp()
  if java_type.is_primitive():
    return ret
  return f'const jni_zero::JavaRef<{ret}>&'


def _param_type_cpp_mirror(native, java_type, use_const=False):
  if java_type.enable_mirror(native.java_class):
    return f'const jni_zero::JavaRef<J{native.java_class.nested_name}>&'
  else:
    return _param_type_cpp_non_mirror(java_type, use_const)


def _impl_forward_declaration(sb, native, params):
  sb('// Forward declaration. To be implemented by the including .cc file.\n')
  with sb.statement():
    name = f'JNI_{native.java_class.name}_{native.capitalized_name}'
    sb(f'static {_return_type_cpp(native.return_type)} {name}')
    with sb.param_list() as plist:
      plist.append('JNIEnv* env')
      if not native.static:
        plist.append('const jni_zero::JavaRef<jobject>& jcaller')
      plist.extend(f'{_param_type_cpp_non_mirror(p.java_type)} {p.cpp_name()}'
                   for p in params)


def _entry_point_example(sb, native):
  if native.first_param_cpp_type:
    name = f'{native.first_param_cpp_type}::'
    params = native.params[1:]
  else:
    name = f'JNI_{native.java_class.name}_'
    params = native.params
  name += native.capitalized_name
  with sb.statement():
    if not native.first_param_cpp_type:
      sb('static ')
    sb(f'{_return_type_cpp(native.return_type)} {name}')
    with sb.param_list() as plist:
      plist.append('JNIEnv* env')
      if not native.static:
        plist.append('const jni_zero::JavaRef<jobject>& jcaller')
      plist.extend(f'{_param_type_cpp_mirror(native, p.java_type, True)} '
                   f'{p.cpp_name()}' for p in params)


def _prep_param(sb, param, native, include_forward_declaration):
  """Returns the snippet to use for the parameter."""
  orig_name = param.cpp_name()
  java_type = param.java_type

  if java_type.converted_type:
    ret = f'{param.name}_converted'
    with sb.statement():
      sb(f'{java_type.converted_type} {ret} = ')
      convert_type.from_jni_expression(sb, orig_name, java_type)
    if not include_forward_declaration:
      ret = f'std::move({ret})'
    return ret

  if java_type.is_primitive():
    return orig_name

  if java_type.enable_mirror(native.java_class):
    cpp_type = f'J{native.java_class.nested_name}'
    orig_name = f'static_cast<{cpp_type}>({orig_name})'
  else:
    cpp_type = java_type.to_cpp()
    if native.is_proxy and cpp_type != java_type.to_proxy().to_cpp():
      # E.g. jobject -> jstring
      orig_name = f'static_cast<{cpp_type}>({orig_name})'

  ret = f'{param.name}_ref'
  with sb.statement():
    sb(f'jni_zero::JavaRef<{cpp_type}> {ret} = ')
    sb(f'jni_zero::JavaRef<{cpp_type}>::CreateLeaky(env, {orig_name})')
  return ret


def _param_type_for_assert_message(param):
  param_type = param.java_type
  if param_type.converted_type:
    return param_type.converted_type
  if param_type.is_primitive():
    return param_type.to_cpp()
  jtype = param_type.to_cpp()
  return f'const jni_zero::JavaRef<{jtype}>&'


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


def entry_point_method(sb,
                       jni_mode,
                       jni_obj,
                       native,
                       gen_jni_class,
                       output_file,
                       include_forward_declaration=False,
                       marker_func_name=None):
  """The method called by JNI, or by multiplexing methods."""
  params = native.params
  cpp_class = native.first_param_cpp_type
  if cpp_class:
    params = params[1:]

  if include_forward_declaration:
    # Only non-class methods need to be forward-declared.
    if not cpp_class:
      _impl_forward_declaration(sb, native, params)
      sb('\n')

  entry_point_declaration(sb, jni_mode, jni_obj, native, gen_jni_class)

  entry_point_return_type = native.entry_point_return_type
  return_type = native.return_type

  if cpp_class:
    func_name = f'{native.capitalized_name}'
    func_name_full = f'{cpp_class}::{func_name}'
  else:
    func_name = f'JNI_{native.java_class.name}_{native.capitalized_name}'
    func_name_full = func_name

  with sb.block(after='\n'):
    if marker_func_name:
      sb('/* Prevent -Wunused-function warning. */\n')
      sb(f'{marker_func_name}();\n')
      sb('\n')

    param_rvalues = [
        _prep_param(sb, param, native, include_forward_declaration)
        for param in params
    ]
    if not native.static:
      param_rvalues.insert(
          0, 'jni_zero::JavaRef<jobject>::CreateLeaky(env, jcaller)')

    if cpp_class:
      with sb.statement():
        sb(f'{cpp_class}* _ptr = reinterpret_cast<{cpp_class}*>'
           f'({native.params[0].cpp_name()})')

    sb('/* Lambda required to make "if constexpr" work properly. */\n')
    sb('auto dependent_context = [&](auto)')
    with sb.block(after=';'):
      sb('/* Lambda required to disambiguate overloads. */\n')
      sb('auto func_wrapper = [&](auto&&... args) ->\n')
      if cpp_class:
        sb(f'decltype(_ptr->{func_name}')
      else:
        sb(f'decltype({func_name}')
      sb(f'(std::forward<decltype(args)>(args)...))')
      with sb.block(after=';'):
        with sb.statement():
          if cpp_class:
            sb(f'return _ptr->{func_name}')
          else:
            sb(f'return {func_name}')
          sb(f'(std::forward<decltype(args)>(args)...)')

      sb('if constexpr (requires { func_wrapper')
      sb.param_list(['env'] + param_rvalues)
      sb('; })')
      with sb.block(no_trailing_newline=True):
        with sb.statement():
          sb('return func_wrapper')
          with sb.param_list() as plist:
            plist.append('env')
            plist.extend(param_rvalues)
      sb(' else if constexpr (requires { func_wrapper')
      sb.param_list(param_rvalues)
      sb('; })')
      with sb.block(no_trailing_newline=True):
        with sb.statement():
          sb('return func_wrapper')
          sb.param_list(param_rvalues)
      sb(' else')
      # Show a custom error message when the signature doesn't match. Without
      # this, the lambda to make overloads work results in a confusing message.
      with sb.block():
        with sb.statement():
          arg_types = [_param_type_for_assert_message(p) for p in params]
          msg = (f'{func_name_full}() is missing or has incorrect signature.\\n'
                 f'It should accept an optional JNIEnv* parameter as well as rvalues of the given types: '
                 f'({", ".join(arg_types)})\\n'
                 f'See {output_file} for an example.')
          sb(f'static_assert(false, "{msg}")')

    with sb.statement():
      if not return_type.is_void():
        sb('auto return_value = ')
      sb('dependent_context(0)')

    if return_type.is_void():
      return

    if not return_type.converted_type:
      if return_type.is_primitive():
        sb('return return_value;\n')
      else:
        # Use ReleaseLocal() to ensure we are not calling .Release() on a
        # global ref. https://crbug.com/40944912
        sb('return return_value.ReleaseLocal();\n')
      return

    with sb.statement():
      sb('auto converted_ret = ')
      if native.needs_implicit_array_element_class_param:
        clazz_snippet = f'static_cast<jclass>({native.proxy_params[-1].name})'
      else:
        clazz_snippet = None
      convert_type.to_jni_expression(sb,
                                     'std::move(return_value)',
                                     return_type,
                                     clazz_snippet=clazz_snippet)
      if not return_type.is_primitive():
        sb('.Release()')

    with sb.statement():
      sb('return ')
      if entry_point_return_type.to_cpp() != 'jobject':
        sb(f'static_cast<{entry_point_return_type.to_cpp()}>(converted_ret)')
      else:
        sb('converted_ret')


def natives_macro_definition(sb, jni_mode, jni_obj, gen_jni_class, output_file, *,
                             enable_definition_macros):
  class_name = jni_obj.java_class.name
  macro_name = f'DEFINE_JNI_FOR_{class_name}'
  marker_func_name = f'YouForgotToCallMacro_DEFINE_JNI_{class_name}'
  if enable_definition_macros and jni_obj.natives:
    with sb.section(
        'Example signatures (to be implemented by #including file).'):
      with sb.commented_section():
        sb('* JNIEnv* parameters are optional.\n')
        sb('* Types do not have to be exact; they must accept rvalues of the examples.\n')
        sb('\n')
        for native in jni_obj.natives:
          _entry_point_example(sb, native)
          sb('\n')

    with sb.section(f'Triggers a compiler warning if DEFINE_JNI({class_name})'
                    ' is forgotten (requires -Wunused-function).'):
      sb(f'static void {marker_func_name}() {{}}\n')

    with sb.section('Java to native functions'):
      with sb.cpp_macro(macro_name):
        # Anonymous namespace to scope the "using namespace" declaration.
        # All symbols use extern "C", so it doesn't actually hide symbols.
        with sb.namespace(''):
          if jni_obj.jni_namespace:
            sb(f'using namespace {jni_obj.jni_namespace};\n')
          for native in jni_obj.natives:
            entry_point_method(sb,
                               jni_mode,
                               jni_obj,
                               native,
                               gen_jni_class,
                               output_file,
                               marker_func_name=marker_func_name)
            # Needs to be included only once to be marked as used.
            marker_func_name = None
  elif enable_definition_macros:
    sb(f'// There are no Java->Native methods.\n')
    sb(f'#define {macro_name}()\n')
  else:
    sb(f'// Macro for transition to enable_definition_macros=true.\n')
    sb(f'#define {macro_name}()\n')


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
      plist.append('int32_t switch_num')
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
