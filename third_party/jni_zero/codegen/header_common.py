# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen common to .h files."""

import common
import java_types


def class_accessors(java_classes, module_name):
  split_arg = f'"{module_name}", ' if module_name else ''
  sb = []
  for java_class in java_classes:
    if java_class in (java_types.OBJECT_CLASS, java_types.STRING_CLASS):
      continue
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


def class_accessor_call(java_class):
  if java_class == java_types.OBJECT_CLASS:
    return 'jni_zero::g_object_class'
  if java_class == java_types.STRING_CLASS:
    return 'jni_zero::g_string_class'

  escaped_name = common.escape_class_name(java_class.full_name_with_slashes)
  return f'{escaped_name}_clazz(env)'
