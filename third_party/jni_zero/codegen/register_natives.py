# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for RegisterNatives() methods."""

from codegen import header_common
import common


def gen_jni_register_function(sb, jni_mode, gen_jni_class,
                              boundary_proxy_natives):
  """RegisterNatives() method for natives in GEN_JNI."""
  sb(f'bool RegisterNative_{gen_jni_class.to_cpp()}(JNIEnv* env)')
  with sb.block():
    sb('static const JNINativeMethod kMethods[] = {\n')
    for i, native in enumerate(boundary_proxy_natives):
      if jni_mode.is_muxing:
        descriptor = native.muxed_signature.to_descriptor()
        if native.muxed_switch_num != -1:
          # Add the switch_num parameter.
          descriptor = '(I' + descriptor[1:]
      else:
        descriptor = native.proxy_signature.to_descriptor()
      boundary_name = native.boundary_name(jni_mode)
      boundary_name_cpp = native.boundary_name_cpp(jni_mode,
                                                   gen_jni_class=gen_jni_class)
      if i > 0:
        sb(' ')
      sb(f"""\
      {{"{boundary_name}",
       "{descriptor}",
       reinterpret_cast<void*>({boundary_name_cpp})}},\n""")
    sb('\n};')
    sb(f"""
jni_zero::ScopedJavaLocalRef<jclass> native_clazz =
    jni_zero::GetClass(env, "{gen_jni_class.full_name_with_slashes}");
if (env->RegisterNatives(native_clazz.obj(), kMethods, std::size(kMethods)) < 0) {{
  jni_zero::internal::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
  return false;
}}

return true;
""")


def non_proxy_register_function(sb, jni_obj):
  """RegisterNatives() method for natives non-proxy or per-class natives."""
  class_accessor = header_common.class_accessor_expression(jni_obj.java_class)
  sb(f'bool RegisterNative_{jni_obj.java_class.to_cpp()}(JNIEnv* env)')
  with sb.block(after='\n'):
    sb(f'static const JNINativeMethod kMethods[] =')
    with sb.block(indent=4, after=';'):
      for i, native in enumerate(jni_obj.non_proxy_natives):
        boundary_name = native.boundary_name(None)
        boundary_name_cpp = native.boundary_name_cpp(None)
        sig = native.proxy_signature if native.is_proxy else native.signature
        if i > 0:
          sb(' ')
        sb(f"""\
{{
    "{boundary_name}",
    "{sig.to_descriptor()}",
    reinterpret_cast<void*>({boundary_name_cpp})
}},""")
      sb('\n')
    sb(f"""\
jclass clazz = {class_accessor};
if (env->RegisterNatives(clazz, kMethods, std::size(kMethods)) < 0) {{
  jni_zero::internal::HandleRegistrationError(env, clazz, __FILE__);
  return false;
}}

return true;
""")


def main_register_function(sb, jni_objs, namespace, gen_jni_class=None):
  """RegisterNatives() that calls the helper RegisterNatives() methods."""
  with sb.namespace(namespace or ''):
    sb('bool RegisterNatives(JNIEnv* env)')
    with sb.block():
      if gen_jni_class:
        sb(f"""// Register natives in a proxy.
if (!RegisterNative_{gen_jni_class.to_cpp()}(env)) {{
  return false;
}}
""")
      for jni_obj in jni_objs:
        if not jni_obj.non_proxy_natives:
          continue
        sb(f"""\
if (!RegisterNative_{jni_obj.java_class.to_cpp()}(env))
  return false;
""")
      sb('\nreturn true;\n')
