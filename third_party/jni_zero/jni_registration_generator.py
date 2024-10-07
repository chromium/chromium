# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "link" command."""

import collections
import functools
import hashlib
import itertools
import multiprocessing
import os
import pathlib
import pickle
import posixpath
import re
import string
import sys
import zipfile

from codegen import gen_jni_java
from codegen import header_common
from codegen import natives_header
from codegen import register_natives
import common
import java_types
import jni_generator
import parse
import proxy


_THIS_DIR = os.path.dirname(__file__)

_SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN = "<INSERT HERE>"

MERGEABLE_KEYS = [
    'PROXY_NATIVE_SIGNATURES',
    'FORWARDING_PROXY_METHODS',
    'PROXY_NATIVE_METHOD_ARRAY',
]


def _ParseHelper(package_prefix, package_prefix_filter, path):
  return parse.parse_java_file(path,
                               package_prefix=package_prefix,
                               package_prefix_filter=package_prefix_filter)


def _LoadJniObjs(paths, options):
  ret = {}
  if all(p.endswith('.jni.pickle') for p in paths):
    for pickle_path in paths:
      with open(pickle_path, 'rb') as f:
        parsed_files = pickle.load(f)
      ret[pickle_path] = [
          jni_generator.JniObject(pf, options, from_javap=False)
          for pf in parsed_files
      ]
  else:
    func = functools.partial(_ParseHelper, options.package_prefix,
                             options.package_prefix_filter)
    with multiprocessing.Pool() as pool:
      for pf in pool.imap_unordered(func, paths):
        ret[pf.filename] = [
            jni_generator.JniObject(pf, options, from_javap=False)
        ]

  return ret


def _FilterJniObjs(jni_objs_by_path, options):
  for jni_objs in jni_objs_by_path.values():
    # Remove test-only methods.
    if not options.include_test_only:
      for jni_obj in jni_objs:
        jni_obj.RemoveTestOnlyNatives()
    # Ignoring non-active modules and empty natives lists.
    jni_objs[:] = [
        o for o in jni_objs
        if o.natives and o.module_name == options.module_name
    ]


def _Flatten(jni_objs_by_path, paths):
  return itertools.chain(*(jni_objs_by_path[p] for p in paths))


def _Generate(options, native_sources, java_sources, priority_java_sources):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides RegisterNatives to perform
  JNI registration.

  Args:
    options: arguments from the command line
    native_sources: A list of .jni.pickle or .java file paths taken from native
        dependency tree. The source of truth.
    java_sources: A list of .jni.pickle or .java file paths. Used to assert
        against native_sources.
    priority_java_sources: A list of .jni.pickle or .java file paths. Used to
        put these listed java files first in multiplexing.
  """
  native_sources_set = set(native_sources)
  java_sources_set = set(java_sources)

  jni_objs_by_path = _LoadJniObjs(native_sources_set | java_sources_set,
                                  options)
  _FilterJniObjs(jni_objs_by_path, options)

  present_jni_objs = list(
      _Flatten(jni_objs_by_path, native_sources_set & java_sources_set))

  # Can contain path not in present_jni_objs.
  priority_set = set(priority_java_sources or [])
  # Sort for determinism and to put priority_java_sources first.
  present_jni_objs.sort(
      key=lambda o: (o.filename not in priority_set, o.java_class))

  whole_hash = None
  priority_hash = None
  if options.enable_jni_multiplexing:
    whole_hash, priority_hash = _GenerateHashes(present_jni_objs, priority_set)

  java_only_jni_objs = _CheckForJavaNativeMismatch(options, jni_objs_by_path,
                                                   native_sources_set,
                                                   java_sources_set)

  short_gen_jni_class = proxy.get_gen_jni_class(
      short=True,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix,
      package_prefix_filter=options.package_prefix_filter)
  full_gen_jni_class = proxy.get_gen_jni_class(
      short=False,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix,
      package_prefix_filter=options.package_prefix_filter)
  if options.use_proxy_hash or options.enable_jni_multiplexing:
    gen_jni_class = short_gen_jni_class
  else:
    gen_jni_class = full_gen_jni_class

  dicts = []
  for jni_obj in present_jni_objs:
    dicts.append(_CreateMuxingDict(jni_obj, options, gen_jni_class))

  combined_dict = {}
  for key in MERGEABLE_KEYS:
    combined_dict[key] = ''.join(d.get(key, '') for d in dicts)

  # PROXY_NATIVE_SIGNATURES and PROXY_NATIVE_METHOD_ARRAY will have
  # duplicates for JNI multiplexing since all native methods with similar
  # signatures map to the same proxy. Similarly, there may be multiple switch
  # case entries for the same proxy signatures.
  if options.enable_jni_multiplexing:
    proxy_signatures_list = sorted(
        set(combined_dict['PROXY_NATIVE_SIGNATURES'].split('\n')))
    combined_dict['PROXY_NATIVE_SIGNATURES'] = '\n'.join(
        signature for signature in proxy_signatures_list)

    proxy_native_array_list = sorted(
        set(combined_dict['PROXY_NATIVE_METHOD_ARRAY'].split('},\n')))
    combined_dict['PROXY_NATIVE_METHOD_ARRAY'] = '},\n'.join(
        p for p in proxy_native_array_list if p != '') + '}'
    signature_to_cpp_calls = collections.defaultdict(list)
    for d in dicts:
      for signature, cases in d['SIGNATURE_TO_CPP_CALLS'].items():
        signature_to_cpp_calls[signature].extend(cases)
    combined_dict['FORWARDING_PROXY_METHODS'] = _InsertMultiplexingSwitchCases(
        signature_to_cpp_calls, combined_dict['FORWARDING_PROXY_METHODS'],
        short_gen_jni_class)
    combined_dict['FORWARDING_CALLS'] = _AddForwardingCalls(
        signature_to_cpp_calls, short_gen_jni_class)

  if options.header_path:
    header_content = _CreateHeader(present_jni_objs, gen_jni_class, options,
                                   combined_dict, whole_hash, priority_hash)
    with common.atomic_output(options.header_path, mode='w') as f:
      f.write(header_content)

  with common.atomic_output(options.srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      if options.use_proxy_hash or options.enable_jni_multiplexing:
        # J/N.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{short_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         gen_jni_class,
                                         combined_dict,
                                         whole_hash=whole_hash,
                                         priority_hash=priority_hash))
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         full_gen_jni_class,
                                         combined_dict,
                                         java_only_jni_objs,
                                         forwarding=True))
      else:
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options, gen_jni_class, combined_dict,
                                         java_only_jni_objs))


def _CheckForJavaNativeMismatch(options, jni_objs_by_path, native_sources_set,
                                java_sources_set):
  native_only = native_sources_set - java_sources_set
  java_only = java_sources_set - native_sources_set

  java_only_jni_objs = sorted(_Flatten(jni_objs_by_path, java_only),
                              key=lambda jni_obj: jni_obj.filename)
  native_only_jni_objs = sorted(_Flatten(jni_objs_by_path, native_only),
                                key=lambda jni_obj: jni_obj.filename)
  failed = False
  if not options.add_stubs_for_missing_native and java_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
We reference Java files which use JNI, but our native library does not depend on
the corresponding generate_jni().
To bypass this check, add stubs to Java with --add-stubs-for-missing-jni.
Excess Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in java_only_jni_objs))
    sys.stderr.write('\n')
  if not options.remove_uncalled_methods and native_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
Our native library depends on generate_jnis which reference Java files that we
do not include in our final dex.
To bypass this check, delete these extra methods with --remove-uncalled-jni.
Unneeded Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in native_only_jni_objs))
    sys.stderr.write('\n')
  if failed:
    sys.exit(1)

  return java_only_jni_objs


def _GenerateHashes(jni_objs, priority_set):
  # We assume that if we have identical files and they are in the same order, we
  # will have switch number alignment. We do this, instead of directly
  # inspecting the switch numbers, because differentiating the priority sources
  # is a big pain.
  whole_hash = hashlib.md5()
  priority_hash = hashlib.md5()
  had_priority = False

  for i, jni_obj in enumerate(jni_objs):
    path = os.path.relpath(jni_obj.filename, start=_THIS_DIR)
    encoded = path.encode('utf-8')
    whole_hash.update(encoded)
    if jni_obj.filename in priority_set:
      had_priority = True
      priority_hash.update(encoded)

  uint64_t_max = (1 << 64) - 1
  int64_t_min = -(1 << 63)
  to_int64 = lambda h: (int(h.hexdigest(), 16) % uint64_t_max) + int64_t_min
  whole_ret = to_int64(whole_hash)

  # Make it clear when there are no priority items.
  priority_ret = to_int64(priority_hash) if had_priority else 0
  return whole_ret, priority_ret


def _InsertMultiplexingSwitchCases(signature_to_cpp_calls,
                                   java_functions_string, short_gen_jni_class):
  switch_case_method_name_re = re.compile(r'return (\w+)\(')
  java_function_call_re = re.compile(r'public static \S+ (\w+)\(')
  method_to_switch_num = {}
  for signature, cases in sorted(signature_to_cpp_calls.items()):
    for i, case in enumerate(cases):
      method_name = switch_case_method_name_re.search(case).group(1)
      method_to_switch_num[method_name] = i
      if len(cases) > 1:
        cases[i] = f'''          case {i}:
             {case}'''

  swaps = {}
  for match in java_function_call_re.finditer(java_functions_string):
    java_name = match.group(1)
    cpp_full_name = 'Java_' + common.escape_class_name(java_name)
    switch_num = method_to_switch_num[cpp_full_name]
    replace_location = java_functions_string.find(
        _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN, match.end())
    swaps[replace_location] = switch_num

  # Doing a seperate pass to construct the new string for efficiency - don't
  # want to do thousands of copies of a massive string.
  new_java_functions_string = ""
  prev_loc = 0
  for loc, num in sorted(swaps.items()):
    new_java_functions_string += java_functions_string[prev_loc:loc]
    new_java_functions_string += str(num)
    prev_loc = loc + len(_SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN)
  new_java_functions_string += java_functions_string[prev_loc:]
  return new_java_functions_string


def _AddForwardingCalls(signature_to_cpp_calls, short_gen_jni_class):
  fn_def_template = string.Template("""
JNI_BOUNDARY_EXPORT ${RETURN} Java_${PROXY_FN_NAME}(
    JNIEnv* env,
    jclass jcaller,
    ${PARAMS_IN_STUB}) {
${FN_BODY}
}""")
  switch_body_template = string.Template("""
        switch (switch_num) {
${CASES}
          default:
            JNI_ZERO_DCHECK(false);
            return${DEFAULT_RETURN};
        }""")

  forwarding_function_definitons = []
  for signature, cases in sorted(signature_to_cpp_calls.items()):
    param_strings, _ = _GetMultiplexingParamsList(signature.param_types)
    java_class_name = short_gen_jni_class.to_cpp()
    java_function_name = common.escape_class_name(
        _GetMultiplexProxyName(signature))
    proxy_function_name = f'{java_class_name}_{java_function_name}'
    all_cases = '\n'.join(cases)
    if len(cases) > 1:
      fn_body = switch_body_template.substitute({
          'PROXY_FN_NAME':
          proxy_function_name,
          'CASES':
          all_cases,
          'DEFAULT_RETURN':
          '' if signature.return_type.is_void() else ' {}',
      })
    else:
      fn_body = f'        {all_cases}'
    forwarding_function_definitons.append(
        fn_def_template.substitute({
            'RETURN': signature.return_type.to_cpp(),
            'PROXY_FN_NAME': proxy_function_name,
            'PARAMS_IN_STUB': ', '.join(param_strings),
            'FN_BODY': fn_body,
        }))

  return ''.join(s for s in forwarding_function_definitons)


def CreateProxyJavaFromDict(options,
                            gen_jni_class,
                            registration_dict,
                            java_only_jni_objs=None,
                            forwarding=False,
                            whole_hash=None,
                            priority_hash=None):
  template = string.Template("""\
// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package ${PACKAGE};

// This file is autogenerated by
//     third_party/jni_zero/jni_registration_generator.py
// Please do not change its content.

public class ${CLASS_NAME} {
${FIELDS}
${METHODS}
}
""")

  if forwarding or not (options.use_proxy_hash
                        or options.enable_jni_multiplexing):
    fields = string.Template("""\
    public static final boolean TESTING_ENABLED = ${TESTING_ENABLED};
    public static final boolean REQUIRE_MOCK = ${REQUIRE_MOCK};
""").substitute({
        'TESTING_ENABLED': str(options.enable_proxy_mocks).lower(),
        'REQUIRE_MOCK': str(options.require_mocks).lower(),
    })
  else:
    if options.enable_jni_multiplexing:
      fields = f'''\
    public static final long WHOLE_HASH = {whole_hash}L;
    public static final long PRIORITY_HASH = {priority_hash}L;
'''
    else:
      fields = ''

  if forwarding:
    methods = registration_dict['FORWARDING_PROXY_METHODS']
  else:
    methods = registration_dict['PROXY_NATIVE_SIGNATURES']

  if java_only_jni_objs:
    methods += gen_jni_java.stubs_for_missing_natives(java_only_jni_objs)

  return template.substitute({
      'CLASS_NAME': gen_jni_class.name,
      'FIELDS': fields,
      'PACKAGE': gen_jni_class.package_with_dots,
      'METHODS': methods
  })


def _CreateHeader(jni_objs, gen_jni_class, options, registration_dict,
                  whole_hash, priority_hash):
  """Returns the content of the header file."""
  header_guard = os.path.splitext(options.header_path)[0].upper() + '_'
  header_guard = re.sub(r'[/.-]', '_', header_guard)

  preamble, epilogue = header_common.header_preamble(
      jni_generator.GetScriptName(),
      gen_jni_class,
      system_includes=['iterator'],  # For std::size().
      user_includes=['third_party/jni_zero/jni_zero_internal.h'],
      header_guard=header_guard)

  module_name = options.module_name or ''

  sb = common.StringBuilder()
  sb.line(preamble)
  if options.enable_jni_multiplexing:
    sb(f"""\
extern const int64_t kJniZeroHash{module_name}Whole = {whole_hash}LL;
extern const int64_t kJniZeroHash{module_name}Priority = {priority_hash}LL;
""")

  non_proxy_natives_java_classes = [
      o.java_class for o in jni_objs if o.non_proxy_natives
  ]
  non_proxy_natives_java_classes.sort()

  if non_proxy_natives_java_classes:
    sb('// Class Accessors.\n')
    header_common.class_accessors(sb, non_proxy_natives_java_classes,
                                  module_name)

  sb('\n// Forward declarations (methods).\n\n')
  for jni_obj in jni_objs:
    for native in jni_obj.natives:
      with sb.statement():
        natives_header.proxy_declaration(sb, jni_obj, native)

  if options.enable_jni_multiplexing:
    sb.line(registration_dict['FORWARDING_CALLS'])

  if options.manual_jni_registration:
    # Helper methods use presence of gen_jni_class to denote presense of proxy
    # methods.
    if not registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
      gen_jni_class = None

    sb('\n// Helper Methods.\n\n')
    with sb.namespace(''):
      if gen_jni_class:
        register_natives.gen_jni_register_function(
            sb, gen_jni_class, registration_dict['PROXY_NATIVE_METHOD_ARRAY'])
      for jni_obj in jni_objs:
        if jni_obj.non_proxy_natives:
          register_natives.per_class_register_function(sb, jni_obj)

    sb('\n// Main Register Function.\n\n')
    register_natives.main_register_function(sb, jni_objs, options.namespace,
                                            gen_jni_class)
  sb(epilogue)
  return sb.to_string()


def _GetMultiplexingParamsList(param_types, java_types=False):
  switch_type = 'int' if java_types else 'jint'
  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params_with_type = [f'{switch_type} switch_num']
  param_names = ['switch_num']
  for t in param_types:
    typename = t.java_type.to_java() if java_types else t.to_cpp()
    params_type_count[typename] += 1
    typename_cleaned = typename.replace('[]', '_array').lower()
    param_name = f'{typename_cleaned}_param{params_type_count[typename]}'
    param_names.append(param_name)
    params_with_type.append(f'{typename} {param_name}')

  return params_with_type, param_names


def _CreateMultiplexedSignature(proxy_signature):
  """Inserts an int parameter as the first parameter."""
  switch_param = java_types.JavaParam(java_types.INT, '_method_idx')
  return java_types.JavaSignature.from_params(
      proxy_signature.return_type,
      java_types.JavaParamList((switch_param, ) + proxy_signature.param_list))


def _GetProxyKMethodArrayEntry(jni_obj, native, gen_jni_class, *,
                               enable_jni_multiplexing, use_proxy_hash):
  jni_descriptor = native.proxy_signature.to_descriptor()
  stub_name = jni_obj.GetStubName(native)

  # Literal name of the native method in the class that contains the actual
  # native declaration.
  if enable_jni_multiplexing:
    class_name = gen_jni_class.to_cpp()
    sorted_signature = native.proxy_signature.with_params_reordered()
    name = _GetMultiplexProxyName(sorted_signature)
    stub_name = f'Java_{class_name}_' + common.escape_class_name(name)
    multipliexed_signature = _CreateMultiplexedSignature(sorted_signature)
    jni_descriptor = multipliexed_signature.to_descriptor()
  elif use_proxy_hash:
    name = native.hashed_proxy_name
  else:
    name = native.proxy_name

  return (f'    {{ "{name}", "{jni_descriptor}", ' +
          f'reinterpret_cast<void*>({stub_name}) }},')


def _GetMuxingCalls(jni_obj):
  # Switch cases are grouped together by the same proxy signatures.
  template = string.Template('return ${STUB_NAME}(env, jcaller${PARAMS});')

  signature_to_cpp_calls = collections.defaultdict(list)
  for native in jni_obj.proxy_natives:
    _, param_names = _GetMultiplexingParamsList(native.proxy_param_types)
    param_string = ', '.join(param_names[1:])
    if param_string:
      param_string = ', ' + param_string
    values = {
        # We are forced to call the generated stub instead of the impl because
        # the impl is not guaranteed to have a globally unique name.
        'STUB_NAME': jni_obj.GetStubName(native),
        'PARAMS': param_string,
    }
    signature = native.proxy_signature.with_params_reordered()
    signature_to_cpp_calls[signature].append(template.substitute(values))
  return signature_to_cpp_calls


def _CreateMuxingDict(jni_obj, options, gen_jni_class):
  ret = {}
  ret['PROXY_NATIVE_METHOD_ARRAY'] = jni_generator.WrapOutput('\n'.join(
      _GetProxyKMethodArrayEntry(
          jni_obj,
          p,
          gen_jni_class,
          enable_jni_multiplexing=options.enable_jni_multiplexing,
          use_proxy_hash=options.use_proxy_hash)
      for p in jni_obj.proxy_natives))

  ret['PROXY_NATIVE_SIGNATURES'] = (''.join(
      _MakeProxySignature(options, native) for native in jni_obj.proxy_natives))

  if options.enable_jni_multiplexing:
    ret['SIGNATURE_TO_CPP_CALLS'] = _GetMuxingCalls(jni_obj)

  if options.use_proxy_hash or options.enable_jni_multiplexing:
    ret['FORWARDING_PROXY_METHODS'] = ('\n'.join(
        _MakeForwardingProxy(options, gen_jni_class, native)
        for native in jni_obj.proxy_natives))

  return ret


_MULTIPLEXED_CHAR_BY_TYPE = {
    '[]': 'A',
    'byte': 'B',
    'char': 'C',
    'double': 'D',
    'float': 'F',
    'int': 'I',
    'long': 'J',
    'Class': 'L',
    'Object': 'O',
    'String': 'R',
    'short': 'S',
    'Throwable': 'T',
    'void': 'V',
    'boolean': 'Z',
}


def _GetShortenedMultiplexingType(type_name):
  # Parameter types could contain multi-dimensional arrays and every
  # instance of [] has to be replaced in the proxy signature name.
  for k, v in _MULTIPLEXED_CHAR_BY_TYPE.items():
    type_name = type_name.replace(k, v)
  return type_name


def _GetMultiplexProxyName(signature):
  # Proxy signatures for methods are named after their return type and
  # parameters to ensure uniqueness, even for the same return types.
  params_list = [
      _GetShortenedMultiplexingType(t.to_java()) for t in signature.param_types
  ]
  params_part = ''
  if params_list:
    params_part = '_' + ''.join(p for p in params_list)

  return_value_part = _GetShortenedMultiplexingType(
      signature.return_type.to_java())
  return '_' + return_value_part + params_part


def _MakeForwardingProxy(options, gen_jni_class, proxy_native):
  template = string.Template("""
    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        ${MAYBE_RETURN}${PROXY_CLASS}.${PROXY_METHOD_NAME}(${PARAM_NAMES});
    }""")


  if options.enable_jni_multiplexing:
    sorted_signature = proxy_native.proxy_signature.with_params_reordered()
    param_names = sorted_signature.param_list.to_call_str()
    if not param_names:
      param_names = _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN
    else:
      param_names = _SWITCH_NUM_TO_BE_INERSERTED_LATER_TOKEN + ', ' + param_names
    proxy_method_name = _GetMultiplexProxyName(sorted_signature)
  else:
    proxy_method_name = proxy_native.hashed_proxy_name
    param_names = proxy_native.proxy_params.to_call_str()

  return template.substitute({
      'RETURN_TYPE':
      proxy_native.proxy_return_type.to_java(),
      'METHOD_NAME':
      proxy_native.proxy_name,
      'PARAMS_WITH_TYPES':
      proxy_native.proxy_params.to_java_declaration(),
      'MAYBE_RETURN':
      '' if proxy_native.proxy_return_type.is_void() else 'return ',
      'PROXY_CLASS':
      gen_jni_class.full_name_with_dots,
      'PROXY_METHOD_NAME':
      proxy_method_name,
      'PARAM_NAMES':
      param_names,
  })


def _MakeProxySignature(options, proxy_native):
  params_with_types = proxy_native.proxy_params.to_java_declaration()
  native_method_line = """
    public static native ${RETURN} ${PROXY_NAME}(${PARAMS_WITH_TYPES});
"""

  if options.enable_jni_multiplexing:
    # This has to be only one line and without comments because all the proxy
    # signatures will be joined, then split on new lines with duplicates removed
    # since multiple |proxy_native|s map to the same multiplexed signature.
    signature_template = string.Template(native_method_line)

    alt_name = None
    sorted_signature = proxy_native.proxy_signature.with_params_reordered()
    proxy_name = _GetMultiplexProxyName(sorted_signature)
    params_with_types_list, _ = _GetMultiplexingParamsList(
        sorted_signature.param_list, java_types=True)
    params_with_types = ', '.join(params_with_types_list)
  elif options.use_proxy_hash:
    signature_template = string.Template("""\
    // Original name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.proxy_name
    proxy_name = proxy_native.hashed_proxy_name
  else:
    signature_template = string.Template("""\
    // Hashed name: ${ALT_NAME}""" + native_method_line)

    # We add the prefix that is sometimes used so that codesearch can find it if
    # someone searches a full method name from the stacktrace.
    alt_name = f'Java_J_N_{proxy_native.hashed_proxy_name}'
    proxy_name = proxy_native.proxy_name

  return_type_str = proxy_native.proxy_return_type.to_java()
  return signature_template.substitute({
      'ALT_NAME': alt_name,
      'RETURN': return_type_str,
      'PROXY_NAME': proxy_name,
      'PARAMS_WITH_TYPES': params_with_types,
  })


def _ParseSourceList(path):
  # Path can have duplicates.
  with open(path) as f:
    return sorted(set(f.read().splitlines()))


def _write_depfile(depfile_path, first_gn_output, inputs):
  def _process_path(path):
    assert not os.path.isabs(path), f'Found abs path in depfile: {path}'
    if os.path.sep != posixpath.sep:
      path = str(pathlib.Path(path).as_posix())
    assert '\\' not in path, f'Found \\ in depfile: {path}'
    return path.replace(' ', '\\ ')

  sb = []
  sb.append(_process_path(first_gn_output))
  if inputs:
    # Sort and uniquify to ensure file is hermetic.
    # One path per line to keep it human readable.
    sb.append(': \\\n ')
    sb.append(' \\\n '.join(sorted(_process_path(p) for p in set(inputs))))
  else:
    sb.append(': ')
  sb.append('\n')

  pathlib.Path(depfile_path).write_text(''.join(sb))


def main(parser, args):
  if not args.enable_proxy_mocks and args.require_mocks:
    parser.error('--require-mocks requires --enable-proxy-mocks.')
  if not args.header_path and args.manual_jni_registration:
    parser.error('--manual-jni-registration requires --header-path.')
  if not args.header_path and args.enable_jni_multiplexing:
    parser.error('--enable-jni-multiplexing requires --header-path.')
  if args.remove_uncalled_methods and not args.native_sources_file:
    parser.error('--remove-uncalled-methods requires --native-sources-file.')
  if args.priority_java_sources_file and not args.enable_jni_multiplexing:
    parser.error('--priority-java-sources is only for multiplexing.')
  if args.enable_jni_multiplexing and args.use_proxy_hash:
    parser.error('--enable-jni-multiplexing cannot work with --use-proxy-hash.')

  java_sources = _ParseSourceList(args.java_sources_file)
  if args.native_sources_file:
    native_sources = _ParseSourceList(args.native_sources_file)
  else:
    if args.add_stubs_for_missing_native:
      # This will create a fully stubbed out GEN_JNI.
      native_sources = []
    else:
      # Just treating it like we have perfect alignment between native and java
      # when only looking at java.
      native_sources = java_sources
  if args.priority_java_sources_file:
    priority_java_sources = _ParseSourceList(args.priority_java_sources_file)
  else:
    priority_java_sources = None

  _Generate(args, native_sources, java_sources, priority_java_sources)

  if args.depfile:
    # GN does not declare a dep on the sources files to avoid circular
    # dependencies, so they need to be listed here.
    all_inputs = native_sources + java_sources + [args.java_sources_file]
    if args.native_sources_file:
      all_inputs.append(args.native_sources_file)
    _write_depfile(args.depfile, args.srcjar_path, all_inputs)
