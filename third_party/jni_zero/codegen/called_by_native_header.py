# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""

from codegen import convert_type
from codegen import header_common
import common
import java_types


def _jni_field_function_name(field, is_setter):
  if field.java_type.is_primitive():
    call = common.capitalize(field.java_type.primitive_name)
  else:
    call = 'Object'
  if field.static:
    call = 'Static' + call
  return f'{"Set" if is_setter else "Get"}{call}Field'


def _field_id_accessor_name(java_class, field):
  return f'{java_class.to_cpp()}_fieldId_{common.jni_mangle(field.name)}'


def field_accessors(sb, java_class, fields):
  for field in fields:
    static_str = 'Static' if field.static else 'Instance'
    field_id_type = f'jni_zero::internal::FieldID::TYPE_{static_str.upper()}'
    accessor_name = _field_id_accessor_name(java_class, field)

    sb(f'inline jfieldID {accessor_name}(JNIEnv* env) {{\n')
    with sb.indent(2):
      sb('static std::atomic<jfieldID> cached_field_id(nullptr);\n')
      class_accessor = header_common.class_accessor_expression(java_class)
      sb(f'jclass clazz = {class_accessor};\n')
      sb('JNI_ZERO_DCHECK(clazz);\n')
      with sb.statement():
        sb(f'jni_zero::internal::InitializeFieldID<{field_id_type}>(env, clazz, '
           f'"{field.name}", "{field.java_type.to_descriptor()}", '
           f'&cached_field_id)')
      sb('return cached_field_id.load(std::memory_order_relaxed);\n')
    sb('}\n\n')


def field_definition(sb, java_class, field):
  static_str = 'Static' if field.static else 'Instance'
  field_id_type = f'jni_zero::internal::FieldID::TYPE_{static_str.upper()}'
  accessor_name = f'Java_{java_class.nested_name}_GetField_{field.name}'

  # Getter
  if field.is_system_class:
    sb('[[maybe_unused]] ')
  sb(f'static {_return_type_cpp(field.java_type)} {accessor_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not field.static:
      plist.append('const jni_zero::JavaRef<jobject>& obj')

  with sb.block(after='\n'):
    if field.const_value is not None:
      value = field.const_value
      if field.java_type == java_types.LONG:
        # C++ parser can't parse MIN_VALUE :P.
        if value.upper() == '-9223372036854775808':
          value = '-9223372036854775807LL - 1LL'
        else:
          value = value + 'LL'
      sb(f'return {value};\n')
      return

    if field.static:
      class_accessor = header_common.class_accessor_expression(java_class)
      sb(f'jclass clazz = {class_accessor};\n')
      sb('JNI_ZERO_DCHECK(clazz);\n')
      receiver = 'clazz'
    else:
      receiver = 'obj.obj()'

    field_id_accessor = _field_id_accessor_name(java_class, field)
    sb(f'jfieldID field_id = {field_id_accessor}(env);\n')
    jni_func_name = _jni_field_function_name(field, False)
    getter_part = f'env->{jni_func_name}({receiver}, field_id)'
    if not (field.java_type.is_primitive() or field.java_type.converted_type):
      jobject_type = field.java_type.to_cpp()
      with sb.statement():
        sb(f'return jni_zero::ScopedJavaLocalRef<{jobject_type}>::Adopt(env, '
           f'static_cast<{jobject_type}>({getter_part}))')
    else:
      with sb.statement():
        if field.java_type.converted_type:
          sb('auto _ret = ')
        else:
          sb('return ')
        sb(getter_part)
      if field.java_type.converted_type:
        with sb.statement():
          sb('return ')
          convert_type.from_jni_expression(sb,
                                           '_ret',
                                           field.java_type,
                                           release_ref=True)

  if not field.final:
    accessor_name = f'Java_{java_class.nested_name}_SetField_{field.name}'
    if field.is_system_class:
      sb('[[maybe_unused]] ')
    sb(f'static void {accessor_name}')
    with sb.param_list() as plist:
      plist.append('JNIEnv* env')
      if not field.static:
        plist.append('const jni_zero::JavaRef<jobject>& obj')
      plist.append(f'{_param_type_cpp(field.java_type)} value')

    with sb.block(after='\n'):
      if field.static:
        class_accessor = header_common.class_accessor_expression(java_class)
        sb(f'jclass clazz = {class_accessor};\n')
        sb('JNI_ZERO_DCHECK(clazz);\n')
        receiver_arg = 'clazz'
      else:
        receiver_arg = 'obj.obj()'

      param_rvalue = 'value'
      if field.java_type.converted_type:
        convert_type.to_jni_assignment(sb, 'converted_value', 'value',
                                       field.java_type)
        param_rvalue = 'converted_value'

      if not field.java_type.is_primitive():
        param_rvalue = f'{param_rvalue}.obj()'
      elif field.java_type.primitive_name == 'int' and not field.java_type.converted_type:
        param_rvalue = f'as_jint({param_rvalue})'

      field_id_accessor = _field_id_accessor_name(java_class, field)
      sb(f'jfieldID field_id = {field_id_accessor}(env);\n')
      with sb.statement():
        sb(f'env->{_jni_field_function_name(field, True)}({receiver_arg}, '
           f'field_id, {param_rvalue})')


def constants_enums(sb, java_class, constant_fields):
  sb(f'enum Java_{java_class.name}_constant_fields {{\n')
  with sb.indent(2):
    for f in constant_fields:
      sb(f'{f.name} = {f.const_value},\n')
  sb('};\n\n')


def _return_type_cpp(return_type):
  if ret := return_type.converted_type:
    return ret
  ret = return_type.to_cpp()
  if not return_type.is_primitive():
    ret = f'jni_zero::ScopedJavaLocalRef<{ret}>'
  return ret


def _param_type_cpp(java_type):
  if type_str := java_type.converted_type:
    if java_type.is_primitive():
      return type_str
    return f'{type_str} const&'
  ret = java_type.to_cpp()
  if java_type.is_primitive():
    if ret == 'jint' or ret == 'int32_t':
      return 'JniIntWrapper'
    return ret
  return f'const jni_zero::JavaRef<{ret}>&'


def _prep_param(sb, param):
  """Returns the snippet to use for the parameter."""
  orig_name = param.cpp_name()
  java_type = param.java_type

  if converted_type := java_type.converted_type:
    converted_name = f'converted_{param.name}'
    convert_type.to_jni_assignment(sb, converted_name, orig_name, java_type)
    orig_name = converted_name

  if java_type.is_primitive():
    if java_type.primitive_name == 'int' and not converted_type:
      return f'as_jint({orig_name})'
    return orig_name
  return f'{orig_name}.obj()'


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


def method_definition(sb, cbn):
  java_class = cbn.java_class
  reciever_arg_is_class = cbn.static or cbn.is_constructor
  if cbn.is_constructor:
    return_type = cbn.java_class.as_type()
  else:
    return_type = cbn.return_type
  is_void = return_type.is_void()

  if cbn.is_system_class:
    sb('[[maybe_unused]] ')
  sb(f'static {_return_type_cpp(return_type)} ')
  sb(f'Java_{java_class.nested_name}_{cbn.method_id_function_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not reciever_arg_is_class:
      plist.append('const jni_zero::JavaRef<jobject>& obj')
    plist.extend(f'{_param_type_cpp(p.java_type)} {p.cpp_name()}'
                 for p in cbn.params)

  with sb.block(after='\n'):
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
    sb(f'jni_zero::internal::JniJavaCallContext<{checked_str}> call_context;\n')
    with sb.statement():
      sb(f'call_context.Init<jni_zero::MethodID::{method_id_type}>')
      sb.param_list([
          'env', 'clazz', f'"{cbn.name}"', f'"{cbn.signature.to_descriptor()}"',
          '&cached_method_id'
      ])

    param_rvalues = [_prep_param(sb, p) for p in cbn.params]

    if not is_void:
      return_rvalue = '_ret'
      sb(f'auto _ret = ')

    with sb.statement():
      sb(f'env->{_jni_function_name(cbn)}')
      sb.param_list([receiver_arg, 'call_context.method_id()'] + param_rvalues)

    if not is_void:
      if return_type.is_primitive() or return_type.converted_type:
        with sb.statement():
          sb('return ')
          if return_type.converted_type:
            convert_type.from_jni_expression(sb,
                                             return_rvalue,
                                             return_type,
                                             release_ref=True)
          else:
            sb(return_rvalue)
        return

      jobject_type = return_type.to_cpp()
      if jobject_type != 'jobject':
        return_rvalue = '_ret2'
        sb(f'{jobject_type} _ret2 = static_cast<{jobject_type}>(_ret);\n')

      with sb.statement():
        sb(f'return jni_zero::ScopedJavaLocalRef<{jobject_type}>::Adopt(env, '
           f'{return_rvalue})')
