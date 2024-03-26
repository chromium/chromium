# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""

from codegen import convert_type
from codegen import header_common
import common


def constants_enums(java_class, constant_fields):
  if not constant_fields:
    return ''
  sb = common.StringBuilder()
  sb(f'// Constants\n')
  sb(f'enum Java_{java_class.name}_constant_fields {{\n')
  with sb.indent(2):
    for c in constant_fields:
      sb(f'{c.name} = {c.value},\n')
  sb('};\n\n')
  return sb.to_string()


def _return_type_cpp(return_type):
  if ret := return_type.converted_type():
    return ret
  ret = return_type.to_cpp()
  if not return_type.is_primitive():
    ret = f'jni_zero::ScopedJavaLocalRef<{ret}>'
  return ret


def _param_type_cpp(java_type):
  if converted_type := java_type.converted_type():
    if java_type.is_primitive():
      return converted_type
    return f'{converted_type} const&'
  ret = java_type.to_cpp()
  if java_type.is_primitive():
    if ret == 'jint':
      return 'JniIntWrapper'
    return ret
  return f'const jni_zero::JavaRef<{ret}>&'


def _param_expression_cpp(param):
  if converted_type := param.java_type.converted_type():
    name = f'{param.name}_converted'
  else:
    name = param.name
  if param.java_type.is_primitive():
    if param.java_type.primitive_name == 'int' and not converted_type:
      return f'as_jint({name})'
    return name
  return f'{name}.obj()'


def _jni_function_name(called_by_native):
  """Maps the types available via env->Call__Method."""
  if called_by_native.is_constructor:
    return 'NewObject'
  if called_by_native.return_type.is_primitive():
    call = common.capitalize(called_by_native.return_type.primitive_name)
  else:
    call = 'Object'
  if called_by_native.static:
    call = 'Static' + call
  return f'Call{call}Method'


def _method_stub(sb, cbn):
  java_class = cbn.java_class
  escaped_name = common.escape_class_name(java_class.full_name_with_slashes)
  reciever_arg_is_class = cbn.static or cbn.is_constructor
  if cbn.is_constructor:
    return_type = cbn.java_class.as_type()
  else:
    return_type = cbn.return_type
  is_void = return_type.is_void()

  if cbn.is_system_class:
    sb('[[maybe_unused]] ')
  sb(f'static {_return_type_cpp(return_type)} ')
  sb(f'Java_{java_class.nested_name}_{cbn.method_id_function_name}(\n')
  with sb.indent(4):
    sb('JNIEnv* env')
    if not reciever_arg_is_class:
      sb(',\nconst jni_zero::JavaRef<jobject>& obj')
    for param in cbn.params:
      sb(f',\n{_param_type_cpp(param.java_type)} {param.name}')
    sb(') {\n')

  with sb.indent(2):
    sb('static std::atomic<jmethodID> cached_method_id(nullptr);\n')
    class_accessor = header_common.class_accessor_expression(java_class)
    receiver_arg = 'clazz' if reciever_arg_is_class else 'obj.obj()'

    sb(f'jclass clazz = {class_accessor};\n')
    if is_void:
      sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz);\n')
    else:
      default_value = return_type.to_cpp_default_value()
      sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz, {default_value});\n')

    checked_str = 'false' if cbn.unchecked else 'true'
    method_id_type = 'TYPE_STATIC' if cbn.static else 'TYPE_INSTANCE'
    sb(f"""
jni_zero::internal::JniJavaCallContext<{checked_str}> call_context;
call_context.Init<jni_zero::MethodID::{method_id_type}>(
    env,
    clazz,
    "{cbn.name}",
    "{cbn.signature.to_descriptor()}",
    &cached_method_id);
""")

    for param in cbn.params:
      if converted_type := param.java_type.converted_type():
        sb(
            convert_type.to_jni_assignment(f'{param.name}_converted',
                                           param.name, param.java_type))

    if not is_void:
      return_rvalue = 'ret'
      sb(f'auto ret = ')

    sb(f"""\
env->{_jni_function_name(cbn)}(
    {receiver_arg},
    call_context.method_id()""")
    for param in cbn.params:
      sb(f',\n    {_param_expression_cpp(param)}')
    sb(');\n')

    if not is_void:
      if not return_type.is_primitive():
        jobject_type = return_type.to_cpp()
        if jobject_type != 'jobject':
          return_rvalue = 'ret2'
          sb(f'{jobject_type} ret2 = static_cast<{jobject_type}>(ret);\n')

      if return_type.converted_type():
        expr = convert_type.from_jni_expression(return_rvalue, return_type)
        sb(f'return {expr};\n')
      elif not return_type.is_primitive():
        sb(f'return jni_zero::ScopedJavaLocalRef<{jobject_type}>(env, '
           f'{return_rvalue});\n')
      else:
        sb(f'return {return_rvalue};\n')
  sb('}\n')


def method_stubs(called_by_natives):
  if not called_by_natives:
    return ''
  sb = common.StringBuilder()
  sb('// Native to Java functions\n')
  for called_by_native in called_by_natives:
    _method_stub(sb, called_by_native)
    sb('\n')
  return sb.to_string()
