# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen common to .h files."""

import common


def class_accessor_snippet(java_classes, module_name):
  split_arg = f'"{module_name}", ' if module_name else ''
  sb = []
  for java_class in java_classes:
    escaped_name = common.escape_class_name(java_class.full_name_with_slashes)
    # #ifdef needed when multple .h files are #included that common classes.
    sb.append(f"""\
#ifndef {escaped_name}_clazz_defined
#define {escaped_name}_clazz_defined
""")
    # Uses std::atomic<> instead of "static jclass cached_class = ..." because
    # that moves the initialize-once logic into the helper method (smaller code
    # size).
    sb.append(f"""\
inline jclass {escaped_name}_clazz(JNIEnv* env) {{
  static const char kClassName[] = "{java_class.full_name_with_slashes}";
  static std::atomic<jclass> cached_class;
  return jni_zero::internal::LazyGetClass(env, kClassName, {split_arg}&cached_class);
}}
#endif

""")
  return ''.join(sb)
