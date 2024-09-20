# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for GEN_JNI.java."""

import common

def _stub_for_missing_native(sb, native):
  sb(f'public static {native.proxy_return_type.to_java()} {native.proxy_name}')
  with sb.param_list() as plist:
    plist.extend(p.to_java_declaration() for p in native.proxy_params)
  with sb.block():
    sb('throw new RuntimeException("Native method not present");\n')


def stubs_for_missing_natives(java_only_jni_objs):
  sb = common.StringBuilder()
  for jni_obj in java_only_jni_objs:
    for native in jni_obj.proxy_natives:
      _stub_for_missing_native(sb, native)
      sb('\n')
  return sb.to_string()
