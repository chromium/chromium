# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logic related to proxying calls through GEN_JNI.java."""

import base64
import collections
import hashlib

import common
import java_types


_MAX_CHARS_FOR_HASHED_NATIVE_METHODS = 8


_MULTIPLEXED_CHAR_BY_TYPE = {
    'byte': 'B',
    'char': 'C',
    'double': 'D',
    'float': 'F',
    'int': 'I',
    'long': 'J',
    'Object': 'O',
    'short': 'S',
    'void': 'V',
    'boolean': 'Z',
}


def _muxed_type_char(java_type):
  return _MULTIPLEXED_CHAR_BY_TYPE[java_type.to_java()]


def muxed_name(muxed_signature):
  # Proxy signatures for methods are named after their return type and
  # parameters to ensure uniqueness, even for the same return types.
  params_list = [_muxed_type_char(t) for t in muxed_signature.param_types]
  params_part = ''
  if params_list:
    params_part = ''.join(p for p in params_list)

  return_value_part = _muxed_type_char(muxed_signature.return_type)
  return return_value_part + params_part


def muxed_signature(proxy_signature):
  sorted_params = sorted(proxy_signature.param_list,
                         key=lambda x: _muxed_type_char(x.java_type))
  param_list = java_types.JavaParamList(sorted_params)
  return java_types.JavaSignature.from_params(proxy_signature.return_type,
                                              param_list)


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

  if package_prefix and common.should_prefix_package('org.jni_zero',
                                                     package_prefix_filter):
    return gen_jni_class.make_prefixed(package_prefix)

  return gen_jni_class


def hashed_name(non_hashed_name, is_test_only):
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


def needs_implicit_array_element_class_param(return_type):
  return (return_type.is_object_array() and return_type.converted_type
          and not return_type.java_class.is_system_class())


def add_implicit_array_element_class_param(signature):
  param = java_types.JavaParam(java_types.OBJECT, '__arrayClazz')
  param_list = java_types.JavaParamList(signature.param_list + (param, ))
  return java_types.JavaSignature.from_params(signature.return_type, param_list)


def populate_muxed_switch_num(jni_objs, *, never_omit_switch_num,
                              include_test_only):
  muxed_aliases_by_sig = collections.defaultdict(list)
  for jni_obj in jni_objs:
    for native in jni_obj.proxy_natives:
      if not include_test_only and native.is_test_only:
        continue
      aliases = muxed_aliases_by_sig[native.muxed_signature]
      native.muxed_switch_num = len(aliases)
      aliases.append(native)
  # Omit switch_num for unique signatures.
  if not never_omit_switch_num:
    for aliases in muxed_aliases_by_sig.values():
      if len(aliases) == 1:
        aliases[0].muxed_switch_num = -1
  return muxed_aliases_by_sig
