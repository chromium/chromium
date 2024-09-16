# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for RegisterNatives() methods."""

from codegen import header_common
import common


def non_proxy_kmethod_array(sb, jni_obj):
  with sb.namespace(jni_obj.jni_namespace or None):
    escaped_class_name = jni_obj.java_class.to_cpp()
    sb(f'static const JNINativeMethod kMethods_{escaped_class_name}[] =')
    with sb.block(indent=4, after=';'):
      for native in jni_obj.non_proxy_natives:
        sb(f"""\
{{
    native{native.cpp_name}",
    "{native.proxy_signature.to_descriptor()}",
    reinterpret_cast<void*>({jni_obj.GetStubName(native)})
}}, """)
      sb('\n')


def gen_jni_register_natives_helper(sb, gen_jni_class, kmethod_entries):
  escaped_gen_jni = gen_jni_class.to_cpp()
  sb(f"""
static const JNINativeMethod kMethods_{escaped_gen_jni}[] = {{
{kmethod_entries}
}};

namespace {{

bool RegisterNative_{escaped_gen_jni}(JNIEnv* env) {{
  const int number_of_methods = std::size(kMethods_{escaped_gen_jni});

  jni_zero::ScopedJavaLocalRef<jclass> native_clazz =
      jni_zero::GetClass(env, "{gen_jni_class.full_name_with_slashes}");
  if (env->RegisterNatives(
      native_clazz.obj(),
      kMethods_{escaped_gen_jni},
      number_of_methods) < 0) {{

    jni_zero::internal::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }}

  return true;
}}

}}  // namespace
""")


def per_file_register_function(sb, jni_obj):
  escaped_class_name = jni_obj.java_class.to_cpp()
  namespace = f'{jni_obj.jni_namespace}::' if jni_obj.jni_namespace else ''
  class_accessor = header_common.class_accessor_expression(jni_obj.java_class)

  sb(f'JNI_ZERO_COMPONENT_BUILD_EXPORT '
     f'bool RegisterNative_{escaped_class_name}(JNIEnv* env)')
  with sb.block(after='\n'):
    sb(f"""\
const int kMethods_{escaped_class_name}Size =
    std::size({namespace}kMethods_{escaped_class_name});
jclass clazz = {class_accessor};
if (env->RegisterNatives(
    clazz,
    {namespace}kMethods_{escaped_class_name},
    kMethods_{escaped_class_name}Size) < 0) {{
  jni_zero::internal::HandleRegistrationError(env, clazz, __FILE__);
  return false;
}}

return true;
""")


def main_register_function(sb, jni_objs, namespace, gen_jni_class=None):
  sb('// Registration function.\n\n')
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
