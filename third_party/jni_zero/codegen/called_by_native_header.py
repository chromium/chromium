# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""

from codegen import convert_type
from codegen import header_common
import common
import java_types


_CPP_RESERVED_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel",
    "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool",
    "break", "case", "catch", "char", "char16_t", "char32_t", "char8_t",
    "class", "compl", "concept", "const", "const_cast", "consteval",
    "constexpr", "constinit", "continue", "contract_assert", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
    "final", "float", "for", "friend", "goto", "if", "import", "inline", "int",
    "long", "module", "mutable", "namespace", "new", "noexcept", "not",
    "not_eq", "nullptr", "operator", "or", "or_eq", "override", "post", "pre",
    "private", "protected", "public", "reflexpr", "register",
    "reinterpret_cast", "replaceable_if_eligible", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch", "synchronized", "template", "this", "thread_local",
    "throw", "transaction_safe", "transaction_safe_dynamic",
    "trivially_relocatable_if_eligible", "true", "try", "typedef", "typeid",
    "typename", "union", "unsigned", "using", "virtual", "void", "volatile",
    "wchar_t", "while", "xor", "xor_eq"
}


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


def _const_value(field):
  value = field.const_value
  if field.java_type == java_types.LONG:
    # C++ parser can't parse MIN_VALUE :P.
    if value == '-9223372036854775808':
      value = '-9223372036854775807LL - 1LL'
    else:
      value = value + 'LL'
  elif field.java_type == java_types.FLOAT:
    if value == 'Infinity':
      value = 'std::numeric_limits<float>::infinity()'
    elif value == '-Infinity':
      value = '-std::numeric_limits<float>::infinity()'
    elif value == 'NaN':
      value = 'std::numeric_limits<float>::quiet_NaN()'
    else:
      value += 'f'
  elif field.java_type == java_types.DOUBLE:
    if value == 'Infinity':
      value = 'std::numeric_limits<double>::infinity()'
    elif value == '-Infinity':
      value = '-std::numeric_limits<double>::infinity()'
    elif value == 'NaN':
      value = 'std::numeric_limits<double>::quiet_NaN()'
  return value


def field_definition(sb, java_class, field):
  static_str = 'Static' if field.static else 'Instance'
  field_id_type = f'jni_zero::internal::FieldID::TYPE_{static_str.upper()}'
  accessor_name = f'Java_{java_class.nested_name}_GetField_{field.name}'

  # Getter
  if field.is_system_class:
    sb('[[maybe_unused]] ')
  sb(f'static {_return_type_cpp_non_mirror(field.java_type)} {accessor_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not field.static:
      plist.append('const jni_zero::JavaRef<jobject>& obj')

  with sb.block(after='\n'):
    if field.const_value is not None:
      sb(f'return {_const_value(field)};\n')
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
      plist.append(f'{_param_type_cpp_non_mirror(field.java_type)} value')

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


def _return_type_cpp_non_mirror(return_type):
  if ret := return_type.converted_type:
    return ret
  ret = return_type.to_cpp()
  if not return_type.is_primitive():
    ret = f'jni_zero::ScopedJavaLocalRef<{ret}>'
  return ret


def _return_type_cpp_mirror_self(cbn):
  """
  Try to make the return type to be ScopedJavaLocalRef<JMyClass> where MyClass
  is the name of the Java class that this @CalledByNative method belongs to.
  Fall back to the non mirror version of the method if unsuccessful.
  Return True if successful, and return False if unsuccessful.
  """
  java_class_name = cbn.java_class.nested_name
  returned_mirrored_class = f'jni_zero::ScopedJavaLocalRef<J{java_class_name}>'
  if cbn.is_constructor:
    return returned_mirrored_class, True
  elif cbn.return_type.enable_mirror(cbn.java_class):
    return returned_mirrored_class, True
  else:
    return _return_type_cpp_non_mirror(cbn.return_type), False


def _return_type_cpp_mirror_others(cbn):
  """
  Try to make the return type to be ScopedJavaLocalRef<JMyClass> where MyClass
  is the name of the Java class that this @CalledByNative method belongs to.
  If unsuccessful, try to make the return type to be
  ScopedJavaLocalRef<JReturnTypeClass> where ReturnTypeClass is the name of
  the Java class of the return type in Java. If still unsuccessful,
  fall back to the non mirror version of the method.
  Return True if returned JReturnTypeClass, and return False otherwise.
  """
  java_class_name = cbn.java_class.nested_name
  returned_mirrored_class = f'jni_zero::ScopedJavaLocalRef<J{java_class_name}>'
  if cbn.is_constructor:
    return returned_mirrored_class, False
  elif cbn.return_type.enable_mirror(cbn.java_class):
    return returned_mirrored_class, False
  elif cbn.return_type.enable_mirror():
    java_class_name = cbn.return_type.java_class.nested_name
    cpp_class_namespace = cbn.return_type.java_class.package_with_colons
    returned_mirrored_class = (f'jni_zero::ScopedJavaLocalRef'
                               f'<::{cpp_class_namespace}::J{java_class_name}>')
    return returned_mirrored_class, True
  else:
    return _return_type_cpp_non_mirror(cbn.return_type), False


def _param_type_cpp_non_mirror(java_type):
  if type_str := java_type.converted_type:
    if java_type.is_primitive():
      return type_str
    if type_str.endswith('&&'):
      return type_str
    return f'{type_str} const&'
  ret = java_type.to_cpp()
  if java_type.is_primitive():
    if ret == 'jint' or ret == 'int32_t':
      return 'JniIntWrapper'
    return ret
  return f'const jni_zero::JavaRef<{ret}>&'


def _param_type_cpp_mirror(java_type):
  if java_type.enable_mirror():
    java_class_name = java_type.java_class.nested_name
    cpp_class_namespace = java_type.java_class.package_with_colons
    return (f'const jni_zero::JavaRef'
            f'<::{cpp_class_namespace}::J{java_class_name}>&')
  return _param_type_cpp_non_mirror(java_type)


def _prep_param(sb, param):
  """Returns the snippet to use for the parameter."""
  ret = param.cpp_name()
  java_type = param.java_type

  if converted_type := java_type.converted_type:
    if not java_type.is_primitive():
      ret = f'std::move({ret})'
    converted_name = f'converted_{param.name}'
    convert_type.to_jni_assignment(sb, converted_name, ret, java_type)
    ret = converted_name

  if java_type.is_primitive():
    if java_type.primitive_name == 'int' and not converted_type:
      return f'as_jint({ret})'
    return ret
  return f'{ret}.obj()'


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


def _sanitize_method_name(method_name_string):
  """
  Add the string 1 to the method name if it is a C++ reserved keyword.
  This is necessary because some strings are reserved keywords in C++ but not
  Java, so we need to ensure these strings are not used in generated C++ code.
  """
  if method_name_string in _CPP_RESERVED_KEYWORDS:
    return method_name_string + '1'
  else:
    return method_name_string


def method_definition(sb, cbn):
  java_class = cbn.java_class
  java_class_name = cbn.java_class.nested_name
  reciever_arg_is_class = cbn.static or cbn.is_constructor
  if cbn.is_constructor:
    return_type = cbn.java_class.as_type()
  else:
    return_type = cbn.return_type
  is_void = return_type.is_void()
  return_type_cpp, returned_mirrored_class = _return_type_cpp_mirror_self(cbn)

  if cbn.is_system_class:
    sb('[[maybe_unused]] ')
  sb(f'static {return_type_cpp} ')
  sb(f'Java_{java_class_name}_{cbn.method_id_function_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not reciever_arg_is_class:
      plist.append('const jni_zero::JavaRef<jobject>& obj')
    plist.extend(f'{_param_type_cpp_non_mirror(p.java_type)} {p.cpp_name()}'
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

      if returned_mirrored_class:
        sb(f'auto _ret3 = static_cast<J{java_class_name}>({return_rvalue});\n')
        return_rvalue = '_ret3'
        jobject_type = f'J{java_class_name}'

      with sb.statement():
        sb(f'return jni_zero::ScopedJavaLocalRef<{jobject_type}>::Adopt(env, '
           f'{return_rvalue})')


def mirrored_cpp_class_lazy_definition(sb,
                                       java_class,
                                       using_jni_namespace=False,
                                       jni_namespace=None):
  java_class_name = java_class.nested_name
  cpp_class_superclass = java_types.CPP_TYPE_BY_JAVA_TYPE.get(
      java_class.full_name_with_slashes, 'jobject')
  cpp_class_namespace = java_class.package_with_colons
  macro_name = (f'_JNI_ZERO_{cpp_class_namespace.replace("::", "_")}'
                f'_J{java_class_name}_DEFINED')

  sb(f'#ifndef {macro_name}\n')
  with sb.namespace(cpp_class_namespace, skip_newline=True):
    sb(f'class _J{java_class_name} : public _{cpp_class_superclass} {{}};\n')
    sb(f'using J{java_class_name} = _J{java_class_name}*;\n')
  sb(f'#define {macro_name}\n')
  sb('#endif\n')
  sb('\n')

  if using_jni_namespace:
    with sb.namespace(jni_namespace):
      sb(f'using J{java_class_name} = ')
      sb(f'::{cpp_class_namespace}::J{java_class_name};\n')
      sb(f'using J{java_class_name}Class = ')
      sb(f'::jni_zero::internal::_CalledByNatives<J{java_class_name}>;\n')
    sb('\n')


def mirrored_cpp_class_actual_implementation(sb, java_class, called_by_natives):
  java_class_name = java_class.nested_name
  sb('template<>\n')
  sb(f'class _CalledByNatives<J{java_class_name}>')
  with sb.block(after=';'):
    sb('public:\n')
    for cbn in called_by_natives:
      mirrored_cpp_method(sb, cbn)
      sb('\n')
  sb('\n')


def mirrored_cpp_method(sb, cbn):
  java_class_name = cbn.java_class.nested_name
  is_static = cbn.static or cbn.is_constructor
  method_name_cpp = _sanitize_method_name(cbn.method_id_function_name)
  return_type_cpp, returned_mirrored_class = _return_type_cpp_mirror_others(cbn)

  if is_static:
    sb('static ')
  sb(f'{return_type_cpp} {method_name_cpp}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    plist.extend(f'{_param_type_cpp_mirror(p.java_type)} {p.cpp_name()}'
                 for p in cbn.params)
  if not is_static:
    sb(' const')

  with sb.block():
    if not is_static:
      sb('auto this_obj = reinterpret_cast')
      sb(f'<const JavaRef<J{java_class_name}>*>(this);\n')

    with sb.statement():
      sb('return ')
      sb(f'Java_{java_class_name}_{cbn.method_id_function_name}')
      with sb.param_list() as plist:
        plist.append('env')
        if not is_static:
          plist.append('*this_obj')
        for p in cbn.params:
          expr = p.cpp_name()
          if java_type := p.java_type.converted_type:
            if not p.java_type.is_primitive():
              expr = f'std::move({expr})'
          plist.append(expr)
      if returned_mirrored_class:
        return_type_java_class_name = cbn.return_type.java_class.nested_name
        return_type_namespace = cbn.return_type.java_class.package_with_colons
        return_type_mirrored_class = (f'::{return_type_namespace}'
                                      f'::J{return_type_java_class_name}')
        sb(f'\n    .As<{return_type_mirrored_class}>()')
