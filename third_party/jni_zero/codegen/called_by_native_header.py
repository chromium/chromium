# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for calling Java methods from C++."""

import json

from codegen import convert_type
from codegen import header_common
import common
import java_types


class _StringPool:
  """Builds a sorted contiguous string pool with embedded null terminators."""

  def __init__(self, name, num_bits, strings):
    assert num_bits in (16, 32)
    self.name = name
    self._offsets = {}
    self._strings = []
    self._num_bits = num_bits

    offset = 0
    for s in sorted(set(strings)):
      self._offsets[s] = offset
      self._strings.append(s)
      offset += len(s.encode('utf-8')) + 1

    max_offset = 2**num_bits
    if offset >= max_offset:
      raise Exception(f'Overflow: {name} string pool size '
                      f'({offset}) exceeds uint{num_bits}_t max ({max_offset})')

  def offset_for(self, s):
    return self._offsets[s]

  def write_offsets_array(self, sb, keys):
    name = self.name.replace('StringPool', 'Offsets')
    assert name != self.name, name
    offsets = self._offsets
    sb(f'extern const uint{self._num_bits}_t {name}[{len(keys)}] = {{')
    for key in keys:
      sb(f'\n    {offsets[key]},')
    sb('};\n')

  def write_values(self, sb):
    strings = self._strings
    if not strings:
      sb(f'extern const char {self.name}[] = "";\n')
    else:
      sb(f'extern const char {self.name}[] =')
      for s in self._strings:
        escaped = s.replace('\\', '\\\\').replace('"', '\\"')
        sb(f'\n    "{escaped}\\0"')
      sb(';\n')


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
      sb(f'::jni_zero::internal::InitializeFieldID<{field_id_type}>('
         f'env, clazz, "{field.name}", "{field.java_type.to_descriptor()}", '
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


def index_decls(sb, jni_classes):
  decls = []
  for jni_class in jni_classes:
    for cbn in jni_class.called_by_natives:
      if not cbn.is_test_only:
        index_var = (f'kCbnIdx_{cbn.muxed_name}')
        decls.append(f'extern const uint16_t {index_var};\n')
  if decls:
    with sb.section('CalledByNative Indices'):
      with sb.namespace('jni_zero::internal'):
        for decl in decls:
          sb(decl)


def registration_metadata(sb, sorted_classes, called_by_natives):
  class_to_index = {c: idx for idx, c in enumerate(sorted_classes)}
  class_names = [c.full_name for c in sorted_classes]
  class_name_pool = _StringPool('kClassNameStringPool', 16, class_names)
  method_name_pool = _StringPool('kMethodNameStringPool', 16,
                                 (cbn.name for cbn in called_by_natives))
  descriptor_pool = _StringPool('kDescriptorStringPool', 32,
                                (cbn.signature.to_descriptor()
                                 for cbn in called_by_natives))
  cbn_details = []
  if called_by_natives:
    for cbn in called_by_natives:
      cls_idx = class_to_index[cbn.java_class]
      name_offset = method_name_pool.offset_for(cbn.name)
      desc_offset = descriptor_pool.offset_for(cbn.signature.to_descriptor())
      cbn_details.append((cls_idx, name_offset, desc_offset))

  classes_count = len(sorted_classes)
  called_by_native_count = len(called_by_natives)

  with sb.namespace('jni_zero::internal'):
    with sb.section('Class Index Definitions.'):
      for idx, java_class in enumerate(sorted_classes):
        index_var = f'kClassIdx_{java_class.to_cpp()}'
        sb(f'extern const uint16_t {index_var} = {idx};\n')

    with sb.section('CalledByNative Table and Indices.'):
      for idx, cbn in enumerate(called_by_natives):
        index_var = f'kCbnIdx_{cbn.muxed_name}'
        sb(f'extern const uint16_t {index_var} = {idx};\n')

    sb(f'std::atomic<jclass> cached_jclasses[{classes_count}];\n')
    sb('std::atomic<jmethodID> '
       f'cached_method_ids[{called_by_native_count}];\n\n')

    class_name_pool.write_values(sb)
    class_name_pool.write_offsets_array(sb, class_names)
    sb('\n')

    sb('extern const CalledByNativeDescriptor '
       f'kCbnDescriptors[{called_by_native_count}] = {{\n')
    with sb.indent(2):
      for cls_idx, name_off, desc_off in cbn_details:
        sb('CalledByNativeDescriptor{'
           f'{cls_idx}, {name_off}, {desc_off}}},\n')
    sb('};\n\n')

    method_name_pool.write_values(sb)
    sb('\n')

    descriptor_pool.write_values(sb)
    sb('\n')

  weak_called_by_natives = [cbn for cbn in called_by_natives if cbn.is_weak]
  if weak_called_by_natives:
    with sb.section('Weak CalledByNative Overrides.'):
      for cbn in weak_called_by_natives:
        _overriding_method_definition(sb, cbn)


def _raw_return_type(cbn):
  return cbn.return_type.to_proxy().to_cpp()


def _raw_params(cbn):
  params = ['JNIEnv* env']
  if not cbn.static:
    params.append('jobject obj')
  params.extend(f'{p.java_type.to_cpp()} {p.cpp_name()}'
                for p in cbn.params.to_proxy())
  return params


def _call_context(sb, cbn, receiver_obj, *, is_muxing):
  java_class = cbn.java_class
  return_type = cbn.return_type
  checked_str = 'false' if cbn.unchecked else 'true'
  if cbn.static and not cbn.is_constructor:
    method_id_type = 'TYPE_STATIC'
  else:
    method_id_type = 'TYPE_INSTANCE'

  if is_muxing:
    index_var = f'kCbnIdx_{cbn.muxed_name}'
    receiver_arg = 'call_context.clazz()' if cbn.static else receiver_obj
    sb(f'::jni_zero::internal::JniJavaCallContext<{checked_str}> '
       f'call_context;\n')
    with sb.statement():
      sb(f'call_context.InitMuxed<::jni_zero::MethodID::{method_id_type}>('
         f'env, ::jni_zero::internal::{index_var})')
    return receiver_arg

  sb('static std::atomic<jmethodID> cached_method_id(nullptr);\n')
  class_accessor = header_common.class_accessor_expression(java_class)
  receiver_arg = 'clazz' if cbn.static else receiver_obj

  sb(f'jclass clazz = {class_accessor};\n')
  if return_type.is_void():
    sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz);\n')
  else:
    default_value = return_type.to_cpp_default_value()
    sb(f'CHECK_CLAZZ(env, {receiver_arg}, clazz, {default_value});\n')

  sb(f'::jni_zero::internal::JniJavaCallContext<{checked_str}> '
     f'call_context;\n')
  with sb.statement():
    sb(f'call_context.Init<::jni_zero::MethodID::{method_id_type}>')
    sb.param_list([
        'env', 'clazz', f'"{cbn.name}"', f'"{cbn.signature.to_descriptor()}"',
        '&cached_method_id'
    ])
  return receiver_arg


def _overriding_method_definition(sb, cbn):
  java_class = cbn.java_class
  raw_func_name = f'JniWeak_{cbn.muxed_name}'
  raw_ret_type = _raw_return_type(cbn)

  sb(f'JNI_ZERO_ALWAYS_INLINE {raw_ret_type} {raw_func_name}')
  with sb.param_list() as plist:
    plist.extend(_raw_params(cbn))

  with sb.block(after='\n'):
    receiver_arg = _call_context(sb, cbn, 'obj', is_muxing=True)

    param_names = [p.cpp_name() for p in cbn.params]
    call_args = [receiver_arg, 'call_context.method_id()'] + param_names

    with sb.statement():
      if cbn.is_constructor:
        sb('return env->NewObject')
        sb.param_list(call_args)
      else:
        if not cbn.return_type.is_void():
          sb('return ')
        sb(f'env->{_jni_function_name(cbn)}')
        sb.param_list(call_args)


def _weak_method_definition(sb, cbn):
  raw_func_name = f'JniWeak_{cbn.muxed_name}'
  raw_ret_type = _raw_return_type(cbn)

  sb(f'[[gnu::weak]] inline {raw_ret_type} {raw_func_name}')
  sb.param_list(_raw_params(cbn))

  with sb.block(after='\n'):
    receiver_arg = _call_context(sb, cbn, 'obj', is_muxing=False)
    param_names = [p.cpp_name() for p in cbn.params]
    call_args = [receiver_arg, 'call_context.method_id()'] + param_names

    with sb.statement():
      if cbn.is_constructor:
        sb('return env->NewObject')
        sb.param_list(call_args)
      else:
        if not cbn.return_type.is_void():
          sb('return ')
        sb(f'env->{_jni_function_name(cbn)}')
        sb.param_list(call_args)


def weak_muxed_methods(sb, jni_classes):
  for jni_class in jni_classes:
    for cbn in jni_class.called_by_natives:
      if not cbn.is_test_only:
        _weak_method_definition(sb, cbn)


def method_definition(sb,
                      jni_class,
                      cbn,
                      *,
                      is_muxing=False,
                      use_weak_called_by_natives=False,
                      allow_unused=False):
  if not is_muxing:
    use_weak_called_by_natives = False
  java_class = jni_class.java_class
  return_type = cbn.return_type
  is_void = return_type.is_void()
  func_name = f'Java_{java_class.nested_name}_{cbn.method_id_function_name}'
  return_type_cpp = _return_type_cpp_non_mirror(return_type)

  if is_muxing:
    sb(f'inline {return_type_cpp} ')
  else:
    if allow_unused or cbn.is_test_only:
      sb('[[maybe_unused]] ')
    sb(f'static {return_type_cpp} ')
  sb(f'{func_name}')
  with sb.param_list() as plist:
    plist.append('JNIEnv* env')
    if not cbn.static:
      plist.append('const ::jni_zero::JavaRef<jobject>& obj')
    plist.extend(f'{_param_type_cpp_non_mirror(p.java_type)} {p.cpp_name()}'
                 for p in cbn.params)

  with sb.block(after='\n'):
    if not use_weak_called_by_natives:
      receiver_arg = _call_context(sb, cbn, 'obj.obj()', is_muxing=is_muxing)

    if use_weak_called_by_natives:
      call_args = ['env']
      if not cbn.static:
        call_args.append('obj.obj()')
    else:
      call_args = [receiver_arg, 'call_context.method_id()']
    call_args.extend(_prep_param(sb, p) for p in cbn.params)

    if not is_void:
      return_rvalue = '_ret'
      sb('auto _ret = ')

    with sb.statement():
      if use_weak_called_by_natives:
        sb(f'::JniWeak_{cbn.muxed_name}')
      else:
        sb(f'env->{_jni_function_name(cbn)}')
      sb.param_list(call_args)

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
        sb(f'return ::jni_zero::ScopedJavaLocalRef<{jobject_type}>::'
           f'Adopt(env, {return_rvalue})')


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
    elif (field.java_type.primitive_name == 'int'
          and not field.java_type.converted_type):
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
