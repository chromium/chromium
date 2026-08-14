# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""

import json

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


def field_accessor(sb, jni_class, field):
  java_class = jni_class.java_class
  static_str = 'Static' if field.static else 'Instance'
  field_id_type = f'::jni_zero::internal::FieldID::TYPE_{static_str.upper()}'
  accessor_name = _field_id_accessor_name(java_class, field)

  sb(f'inline jfieldID {accessor_name}(JNIEnv* env) {{\n')
  with sb.indent(2):
    sb('static std::atomic<jfieldID> cached_field_id(nullptr);\n')
    class_accessor = header_common.class_accessor_expression(java_class)
    sb(f'jclass clazz = {class_accessor};\n')
    sb('JNI_ZERO_DCHECK(clazz);\n')
    with sb.statement():
      sb(f'::jni_zero::internal::InitializeFieldID<{field_id_type}>(env, clazz, '
         f'"{field.name}", "{field.java_type.to_descriptor()}", '
         f'&cached_field_id)')
    sb('return cached_field_id.load(std::memory_order_relaxed);\n')
  sb('}\n\n')


def _const_value(field):
  value = field.const_value
  if field.java_type.is_string():
    return json.dumps(value)
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


def _return_type_cpp_non_mirror(return_type):
  if ret := return_type.converted_type:
    return ret
  ret = return_type.to_cpp()
  if not return_type.is_primitive():
    ret = f'::jni_zero::ScopedJavaLocalRef<{ret}>'
  return ret


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
  return f'const ::jni_zero::JavaRef<{ret}>&'


def _param_type_cpp_mirror(java_type):
  if java_type.enable_mirror():
    jobject_type = java_type.to_mirror_cpp()
    return (f'const ::jni_zero::JavaRef<{jobject_type}>&')
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


def method_definition(sb, jni_class, cbn, *, allow_unused):
  java_class = jni_class.java_class
  java_class_name = java_class.nested_name
  return_type = cbn.return_type
  is_void = return_type.is_void()
  return_type_cpp = _return_type_cpp_non_mirror(return_type)

  # Mirror classes use these functions, but if a mirror function is templated,
  # it does not count as a usage unless the template is instantiated.
  if allow_unused:
    sb('[[maybe_unused]] ')
  sb(f'static {return_type_cpp} ')
  sb(f'Java_{java_class_name}_{cbn.method_id_function_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not cbn.static:
      plist.append('const ::jni_zero::JavaRef<jobject>& obj')
    plist.extend(f'{_param_type_cpp_non_mirror(p.java_type)} {p.cpp_name()}'
                 for p in cbn.params)

  with sb.block(after='\n'):
    sb('static std::atomic<jmethodID> cached_method_id(nullptr);\n')
    class_accessor = header_common.class_accessor_expression(java_class)
    receiver_arg = 'clazz' if cbn.static else 'obj.obj()'

    sb(f'jclass clazz = {class_accessor};\n')
    if is_void:
      sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz);\n')
    else:
      default_value = return_type.to_cpp_default_value()
      sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz, {default_value});\n')

    checked_str = 'false' if cbn.unchecked else 'true'
    sb(f'::jni_zero::internal::JniJavaCallContext<{checked_str}> '
       f'call_context;\n')
    with sb.statement():
      if cbn.static and not cbn.is_constructor:
        method_id_type = 'TYPE_STATIC'
      else:
        method_id_type = 'TYPE_INSTANCE'
      sb(f'call_context.Init<::jni_zero::MethodID::{method_id_type}>')
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
        sb(f'return ::jni_zero::ScopedJavaLocalRef<{jobject_type}>::Adopt(env, '
           f'{return_rvalue})')


def _gen_t_names(generics):
  if not generics:
    return '', ''
  if isinstance(generics, java_types.JavaTypeParamList):
    names = [p.name for p in generics]
  elif len(generics) == 1:
    names = ('T', )
  else:
    names = [f'T{n}' for n in range(1, len(generics) + 1)]
  typename_list = ', '.join(f'typename {t}' for t in names)
  template_arglist = '<' + ', '.join(names) + '>'
  template_decl = f'template <{typename_list}>\n'
  return template_decl, template_arglist


def _global_class_alias(sb, namespace, name):
  with sb.ifndef(f'_JNI_ZERO_{name}_DEFINED'):
    sb(f'using ::{namespace}::{name};\n')


def jobject_subclass_definition(sb, java_type):
  # Don't generated mirror classes for JObject / JThrowable / JString.
  # Use jobject / jthrowable / jstring directly.
  if not java_type.enable_mirror():
    return
  java_class = java_type.java_class
  jobject_name = java_class.jobject_name
  package_with_underscores = java_class.package_with_underscores
  definition_macro_name = (
      f'_JNI_ZERO_{package_with_underscores}_{jobject_name}_DEFINED')

  template_decl, template_arglist = _gen_t_names(java_type.generics)
  with sb.ifndef(definition_macro_name):
    with sb.namespace(java_class.mirror_namespace, skip_newline=True):
      sb(template_decl.replace('typename', '::jni_zero::internal::IsJobject'))
      sb(f'class _{jobject_name} : public _jobject {{}};\n')

      sb(template_decl)
      sb(f'using {jobject_name} = _{jobject_name}{template_arglist}*;\n')

  # Alias type into the global namespace.
  _global_class_alias(sb, java_class.mirror_namespace, jobject_name)


def called_by_natives_alias(sb, jni_class):
  java_class = jni_class.java_class
  name = f'{java_class.name_with_underscores}Jni'
  template_arglist = ''
  if jni_class.type_params:
    template_arglist = '<%s>' % ', '.join('jobject'
                                          for p in jni_class.type_params)
  qualified_name = java_class.to_mirror_cpp() + template_arglist
  # Create the alias in the pacakge namespace.
  with sb.namespace(java_class.mirror_namespace, skip_newline=True):
    sb(f'using {name} = '
       f'::jni_zero_internal::_CalledByNativesStatics<{qualified_name}>;\n')

  # Alias type into the global namespace.
  _global_class_alias(sb, java_class.mirror_namespace, name)


def called_by_natives_specialization(sb, jni_class, *, is_static):
  java_class = jni_class.java_class
  java_type = jni_class.java_type
  # Static methods in java classes do not use class type params.
  type_params = () if is_static else jni_class.type_params
  template_decl, template_arglist = _gen_t_names(type_params)
  if template_arglist:
    sb(template_decl)
  else:
    sb('template<>\n')

  if is_static and jni_class.type_params:
    template_arglist = '<%s>' % ', '.join('jobject'
                                          for p in jni_class.type_params)

  qualified = java_class.to_mirror_cpp() + template_arglist
  class_suffix = 'Statics' if is_static else ''
  sb(f'class _CalledByNatives{class_suffix}<{qualified}>')
  with sb.block(after=';'):
    sb('public:\n')
    for f in jni_class.fields:
      if f.static == is_static and f.const_value is not None:
        if f.java_type.is_string():
          sb(f'static inline constexpr char {f.name}[] = {_const_value(f)};\n')
        else:
          sb(f'static inline constexpr {f.java_type.to_cpp()} '
             f'{f.name} = {_const_value(f)};\n')
    for f in jni_class.fields:
      if f.static == is_static:
        _mirrored_field_getter(sb, java_type, f)
        if not f.final:
          _mirrored_field_setter(sb, java_type, f)
    for cbn in jni_class.called_by_natives:
      if cbn.static == is_static:
        _mirrored_cpp_function(sb, java_type, cbn)
        sb('\n')
  sb('\n')


def _mirrored_field_getter(sb, java_type, field):
  jobject_type = java_type.to_mirror_cpp()
  java_class = java_type.java_class
  if field.java_type.enable_mirror():
    return_jobject_type = field.java_type.to_mirror_cpp()
    return_type_cpp = f'::jni_zero::ScopedJavaLocalRef<{return_jobject_type}>'
  else:
    return_jobject_type = field.java_type.to_cpp()
    return_type_cpp = _return_type_cpp_non_mirror(field.java_type)

  if field.static:
    sb('static ')

  sb(f'{return_type_cpp} Get_{field.name}(JNIEnv* env)')
  if not field.static:
    sb(' const')

  with sb.block():
    if field.const_value is not None and field.java_type.is_primitive():
      sb(f'return {_const_value(field)};\n')
      return

    if field.static:
      class_accessor = header_common.class_accessor_expression(java_class)
      sb(f'jclass clazz = {class_accessor};\n')
      sb('JNI_ZERO_DCHECK(clazz);\n')
      receiver = 'clazz'
    else:
      sb('auto this_obj = reinterpret_cast')
      sb(f'<const ::jni_zero::JavaRef<{jobject_type}>*>(this);\n')
      receiver = 'this_obj->obj()'

    field_id_accessor = _field_id_accessor_name(java_class, field)
    sb(f'jfieldID field_id = {field_id_accessor}(env);\n')
    jni_func_name = _jni_field_function_name(field, False)
    getter_part = f'env->{jni_func_name}({receiver}, field_id)'

    with sb.statement():
      sb('return ')
      if field.java_type.is_primitive():
        sb(getter_part)
        return

      sb(f'::jni_zero::ScopedJavaLocalRef<>::Adopt(env, {getter_part})')
      if return_jobject_type != 'jobject':
        template_keyword = 'template ' if java_type.generics else ''
        sb(f'\n    .{template_keyword}As<{return_jobject_type}>()')


def _mirrored_field_setter(sb, java_type, field):
  jobject_type = java_type.to_mirror_cpp()
  java_class = java_type.java_class
  param_type = _param_type_cpp_mirror(field.java_type)
  if field.static:
    sb('static ')

  sb(f'void Set_{field.name}(JNIEnv* env, {param_type} value)')
  if not field.static:
    sb(' const')

  with sb.block():
    if field.static:
      class_accessor = header_common.class_accessor_expression(java_class)
      sb(f'jclass clazz = {class_accessor};\n')
      sb('JNI_ZERO_DCHECK(clazz);\n')
      receiver_arg = 'clazz'
    else:
      sb('auto this_obj = reinterpret_cast')
      sb(f'<const ::jni_zero::JavaRef<{jobject_type}>*>(this);\n')
      receiver_arg = 'this_obj->obj()'

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


def _mirrored_cpp_function(sb, java_type, cbn):
  jobject_type = java_type.to_mirror_cpp()

  template_decl, _ = _gen_t_names(cbn.type_params)
  sb(template_decl)

  if cbn.is_constructor:
    return_jobject_type = jobject_type
    return_type_cpp = f'::jni_zero::ScopedJavaLocalRef<{return_jobject_type}>'
  elif cbn.return_type.enable_mirror():
    return_jobject_type = cbn.return_type.to_mirror_cpp()
    return_type_cpp = f'::jni_zero::ScopedJavaLocalRef<{return_jobject_type}>'
  else:
    return_jobject_type = None
    return_type_cpp = _return_type_cpp_non_mirror(cbn.return_type)

  if cbn.static:
    sb('static ')
  sb(f'{return_type_cpp} {cbn.mirrored_function_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    plist.extend(f'{_param_type_cpp_mirror(p.java_type)} {p.cpp_name()}'
                 for p in cbn.params)
  if not cbn.static:
    sb(' const')

  with sb.block():
    if not cbn.static:
      sb('auto this_obj = reinterpret_cast')
      sb(f'<const ::jni_zero::JavaRef<{jobject_type}>*>(this);\n')

    with sb.statement():
      if not cbn.return_type.is_void():
        sb('return ')
      java_class_name = java_type.java_class.nested_name
      sb(f'Java_{java_class_name}_{cbn.method_id_function_name}')
      with sb.param_list() as plist:
        plist.append('env')
        if not cbn.static:
          plist.append('*this_obj')
        for p in cbn.params:
          expr = p.cpp_name()
          if p.java_type.converted_type:
            if not p.java_type.is_primitive():
              expr = f'std::move({expr})'
          plist.append(expr)

      if return_jobject_type:
        template_keyword = ''
        if cbn.type_params or java_type.generics:
          template_keyword = 'template '
        sb(f'\n    .{template_keyword}As<{return_jobject_type}>()')
