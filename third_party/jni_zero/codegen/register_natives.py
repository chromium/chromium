# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for RegisterNatives() methods."""

from codegen import header_common
import common


def gen_jni_register_function(sb, gen_jni_class, kmethod_entries):
  sb(f"""\
bool RegisterNative_{gen_jni_class.to_cpp()}(JNIEnv* env) {{
  static const JNINativeMethod kMethods[] = {{
{kmethod_entries}
  }};

  jni_zero::ScopedJavaLocalRef<jclass> native_clazz =
      jni_zero::GetClass(env, "{gen_jni_class.full_name_with_slashes}");
  if (env->RegisterNatives(native_clazz.obj(), kMethods, std::size(kMethods)) < 0) {{
    jni_zero::internal::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }}

  return true;
}}
""")


def per_class_register_function(sb, jni_obj):
  class_accessor = header_common.class_accessor_expression(jni_obj.java_class)
  sb(f'bool RegisterNative_{jni_obj.java_class.to_cpp()}(JNIEnv* env)')
  with sb.block(after='\n'):
    sb(f'static const JNINativeMethod kMethods[] =')
    with sb.block(indent=4, after=';'):
      for native in jni_obj.non_proxy_natives:
        sb(f"""\
{{
    "native{native.cpp_name}",
    "{native.proxy_signature.to_descriptor()}",
    reinterpret_cast<void*>({jni_obj.GetStubName(native)})
}}, """)
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
