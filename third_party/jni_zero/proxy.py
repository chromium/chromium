# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logic related to proxying calls through GEN_JNI.java."""

import base64
import hashlib

import common
import java_types

# 'Proxy' native methods are declared in an @NativeMethods interface without
# a native qualifier and indicate that the JNI annotation processor should
# generate code to link between the equivalent native method as if it were
# declared statically.

_MAX_CHARS_FOR_HASHED_NATIVE_METHODS = 8


def get_gen_jni_class(*,
                      short=False,
                      name_prefix=None,
                      package_prefix=None,
                      package_prefix_filter=None):
  """Returns the JavaClass for GEN_JNI."""
  package = 'J' if short else 'org/jni_zero'
  name_prefix = name_prefix + '_' if name_prefix else ''
  name = name_prefix + ('N' if short else 'GEN_JNI')
  gen_jni_class = java_types.JavaClass(f'{package}/{name}')

  if package_prefix and common.should_rename_package('org.jni_zero',
                                                     package_prefix_filter):
    return gen_jni_class.make_prefixed(package_prefix)

  return gen_jni_class


def create_hashed_method_name(non_hashed_name, is_test_only):
  md5 = hashlib.md5(non_hashed_name.encode('utf8')).digest()
  hash_b64 = base64.b64encode(md5, altchars=b'$_').decode('utf-8')

  long_hash = ('M' + hash_b64).rstrip('=')
  hashed_name = long_hash[:_MAX_CHARS_FOR_HASHED_NATIVE_METHODS]

  # If the method is a test-only method, we don't care about saving size on
  # the method name, since it shouldn't show up in the binary. Additionally,
  # if we just hash the name, our checkers which enforce that we have no
  # "ForTesting" methods by checking for the suffix "ForTesting" will miss
  # these. We could preserve the name entirely and not hash anything, but
  # that risks collisions. So, instead, we just append "ForTesting" to any
  # test-only hashes, to ensure we catch any test-only methods that
  # shouldn't be in our final binary.
  if is_test_only:
    return hashed_name + '_ForTesting'
  return hashed_name


def create_method_names(java_class, method_name, is_test_only):
  """Returns the method name used in GEN_JNI (both hashed an non-hashed)."""
  proxy_name = common.escape_class_name(
      f'{java_class.full_name_with_slashes}/{method_name}')
  hashed_proxy_name = create_hashed_method_name(proxy_name, is_test_only)
  return proxy_name, hashed_proxy_name


def needs_implicit_array_element_class_param(return_type):
  return (return_type.is_object_array() and return_type.converted_type
          and not return_type.java_class.is_system_class())


def add_implicit_array_element_class_param(signature):
  param = java_types.JavaParam(java_types.CLASS, '__arrayClazz')
  param_list = java_types.JavaParamList(signature.param_list + (param, ))
  return java_types.JavaSignature.from_params(signature.return_type, param_list)
