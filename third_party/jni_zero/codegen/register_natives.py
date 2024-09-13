# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for RegisterNatives() methods."""

import common


def gen_jni_register_natives_helper(sb, name, gen_jni_class, kmethod_entries):
  escaped_gen_jni = common.escape_class_name(
      gen_jni_class.full_name_with_slashes)
  sb(f"""
static const JNINativeMethod kMethods_{escaped_gen_jni}[] = {{
{kmethod_entries}
}};

namespace {{

bool {name}(JNIEnv* env) {{
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
