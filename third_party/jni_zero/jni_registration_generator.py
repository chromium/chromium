# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "link" command."""

import collections
import functools
import hashlib
import multiprocessing
import os
import re
import string
import sys
import zipfile

import common
import java_types
import jni_generator
import parse
import proxy

from util import build_utils
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers

# All but FULL_CLASS_NAME, which is used only for sorting.
MERGEABLE_KEYS = [
    'CLASS_PATH_DECLARATIONS',
    'FORWARD_DECLARATIONS',
    'JNI_NATIVE_METHOD',
    'JNI_NATIVE_METHOD_ARRAY',
    'PROXY_NATIVE_SIGNATURES',
    'FORWARDING_PROXY_METHODS',
    'PROXY_NATIVE_METHOD_ARRAY',
    'REGISTER_NATIVES',
]


def _Generate(options, native_sources, java_sources):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides RegisterNatives to perform
  JNI registration.

  Args:
    options: arguments from the command line
    native_sources: A list of java file paths. The source of truth.
    java_sources: A list of java file paths. Used to assert against
      native_sources.
  """
  # Without multiprocessing, script takes ~13 seconds for chrome_public_apk
  # on a z620. With multiprocessing, takes ~2 seconds.
  results = []
  cached_results_for_stubs = {}
  with multiprocessing.Pool() as pool:
    # The native-based sources are the "source of truth" - the Java based ones
    # will be used later to generate stubs and make assertions.
    for result in pool.imap_unordered(functools.partial(_DictForPath, options),
                                      native_sources):
      d, path, jni_obj = result
      if d:
        results.append(d)
      cached_results_for_stubs[path] = jni_obj

  native_sources_set = {d['FILE_PATH'] for d in results}
  java_sources_set = set(java_sources)
  stub_dicts = _GenerateStubsAndAssert(options, native_sources_set,
                                       java_sources_set,
                                       cached_results_for_stubs)
  # Sort to make output deterministic.
  results.sort(key=lambda d: d['FULL_CLASS_NAME'])

  combined_dict = {}
  for key in MERGEABLE_KEYS:
    combined_dict[key] = ''.join(d.get(key, '') for d in results)

  short_gen_jni_class = proxy.get_gen_jni_class(
      short=True,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix)
  full_gen_jni_class = proxy.get_gen_jni_class(
      short=False,
      name_prefix=options.module_name,
      package_prefix=options.package_prefix)
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
    signature_to_cases = collections.defaultdict(list)
    for d in results:
      for signature, cases in d['SIGNATURE_TO_CASES'].items():
        signature_to_cases[signature].extend(cases)
    combined_dict['FORWARDING_CALLS'] = _AddForwardingCalls(
        signature_to_cases, short_gen_jni_class)

  if options.header_path:
    header_guard = os.path.splitext(options.header_path)[0].upper() + '_'
    header_guard = re.sub(r'[/.-]', '_', header_guard)
    combined_dict['HEADER_GUARD'] = header_guard
    combined_dict['NAMESPACE'] = options.namespace or ''
    header_content = CreateFromDict(options, combined_dict)
    with action_helpers.atomic_output(options.header_path, mode='w') as f:
      f.write(header_content)

  stub_methods_string = ''.join(d['STUBS'] for d in stub_dicts)

  with action_helpers.atomic_output(options.srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      if options.use_proxy_hash or options.enable_jni_multiplexing:
        gen_jni_class = short_gen_jni_class
      else:
        gen_jni_class = full_gen_jni_class

      if options.use_proxy_hash or options.enable_jni_multiplexing:
        # J/N.java
        zip_helpers.add_to_zip_hermetic(
            srcjar,
            f'{short_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options, gen_jni_class, combined_dict))
        # org/chromium/base/natives/GEN_JNI.java
        zip_helpers.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         full_gen_jni_class,
                                         combined_dict,
                                         stub_methods=stub_methods_string,
                                         forwarding=True))
      else:
        # org/chromium/base/natives/GEN_JNI.java
        zip_helpers.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=CreateProxyJavaFromDict(options,
                                         gen_jni_class,
                                         combined_dict,
                                         stub_methods=stub_methods_string))


def _GenerateStubsAndAssert(options, native_sources_set, java_sources_set,
                            cached_results_for_stubs):
  native_only = native_sources_set - java_sources_set
  java_only = java_sources_set - native_sources_set
  # Using stub_only because we just need this to do a boolean check to see if
  # the files have JNI - we don't need any actual output.
  dict_by_path = {
      f: _DictForPath(options,
                      f,
                      stub_only=True,
                      cached_results_for_stubs=cached_results_for_stubs)[0]
      for f in java_only
  }
  dict_by_path = {k: v for k, v in sorted(dict_by_path.items()) if v}
  failed = False
  if not options.add_stubs_for_missing_native and dict_by_path:
    failed = True
    warning_message = '''Failed JNI assertion!
We reference Java files which use JNI, but our native library does not depend on
the corresponding generate_jni().
To bypass this check, you can add stubs to Java with add_stubs_for_missing_jni.
Excess Java files below:
'''
    warning_message += ', '.join(dict_by_path)
    sys.stderr.write(warning_message)
  if not options.remove_uncalled_methods and native_only:
    failed = True
    warning_message = '''Failed JNI assertion!
Our native library depends on generate_jnis which reference Java files that we
do not include in our final dex.
To bypass this check, delete these extra JNI methods with remove_uncalled_jni.
Unneeded Java files below:
'''
    warning_message += str(native_only)
    sys.stderr.write(warning_message)
  if failed:
    sys.exit(1)
  return list(dict_by_path.values())


def _DictForPath(options, path, stub_only=False, cached_results_for_stubs=None):
  # The cached results are generated by the real runs, which happen first, so
  # the cache is only for the stub checks after.
  assert (cached_results_for_stubs is not None) == stub_only
  jni_obj = stub_only and cached_results_for_stubs.get(path)
  if not jni_obj:
    parsed_file = parse.parse_java_file(path,
                                        package_prefix=options.package_prefix)
    jni_obj = jni_generator.JNIFromJavaSource(parsed_file, options)
    if not options.include_test_only:
      jni_obj.RemoveTestOnlyNatives()

  if jni_obj.module_name != options.module_name:
    # Ignoring any code from modules we aren't looking at.
    return None, path, jni_obj

  if not jni_obj.natives:
    return None, path, jni_obj
  if stub_only:
    return {'STUBS': _GenerateStubs(jni_obj.proxy_natives)}, path, jni_obj

  # The namespace for the content is separate from the namespace for the
  # generated header file.
  dict_generator = DictionaryGenerator(jni_obj, options)
  return dict_generator.Generate(), path, jni_obj


def _GenerateStubs(natives):
  final_string = ''
  for native in natives:
    template = string.Template("""

    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        throw new RuntimeException("Stub - not implemented!");
    }""")

    final_string += template.substitute({
        'RETURN_TYPE':
        native.proxy_return_type.to_java(),
        'METHOD_NAME':
        native.proxy_name,
        'PARAMS_WITH_TYPES':
        native.proxy_params.to_java_declaration(),
    })
  return final_string


def _AddForwardingCalls(signature_to_cases, short_gen_jni_class):
  template = string.Template("""
JNI_GENERATOR_EXPORT ${RETURN} Java_${CLASS_NAME}_${PROXY_SIGNATURE}(
    JNIEnv* env,
    jclass jcaller,
    ${PARAMS_IN_STUB}) {
        switch (switch_num) {
          ${CASES}
          default:
            CHECK(false) << "JNI multiplexing function Java_\
${CLASS_NAME}_${PROXY_SIGNATURE} was called with an invalid switch number: "\
 << switch_num;
            return${DEFAULT_RETURN};
        }
}""")

  switch_statements = []
  for signature, cases in sorted(signature_to_cases.items()):
    params_in_stub = _GetJavaToNativeParamsList(signature.param_types)
    switch_statements.append(
        template.substitute({
            'RETURN':
            signature.return_type.to_cpp(),
            'CLASS_NAME':
            common.escape_class_name(
                short_gen_jni_class.full_name_with_slashes),
            'PROXY_SIGNATURE':
            common.escape_class_name(_GetMultiplexProxyName(signature)),
            'PARAMS_IN_STUB':
            params_in_stub,
            'CASES':
            ''.join(cases),
            'DEFAULT_RETURN':
            '' if signature.return_type.is_void() else ' {}',
        }))

  return ''.join(s for s in switch_statements)


def _SetProxyRegistrationFields(options, gen_jni_class, registration_dict):
  registration_template = string.Template("""\

static const JNINativeMethod kMethods_${ESCAPED_PROXY_CLASS}[] = {
${KMETHODS}
};

namespace {

JNI_REGISTRATION_EXPORT bool ${REGISTRATION_NAME}(JNIEnv* env) {
  const int number_of_methods = std::size(kMethods_${ESCAPED_PROXY_CLASS});

  base::android::ScopedJavaLocalRef<jclass> native_clazz =
      base::android::GetClass(env, "${PROXY_CLASS}");
  if (env->RegisterNatives(
      native_clazz.obj(),
      kMethods_${ESCAPED_PROXY_CLASS},
      number_of_methods) < 0) {

    jni_generator::HandleRegistrationError(env, native_clazz.obj(), __FILE__);
    return false;
  }

  return true;
}

}  // namespace
""")

  registration_call = string.Template("""\

  // Register natives in a proxy.
  if (!${REGISTRATION_NAME}(env)) {
    return false;
  }
""")

  manual_registration = string.Template("""\
// Step 3: Method declarations.

${JNI_NATIVE_METHOD_ARRAY}\
${PROXY_NATIVE_METHOD_ARRAY}\

${JNI_NATIVE_METHOD}
// Step 4: Registration function.

namespace ${NAMESPACE} {

bool RegisterNatives(JNIEnv* env) {\
${REGISTER_PROXY_NATIVES}
${REGISTER_NATIVES}
  return true;
}

}  // namespace ${NAMESPACE}
""")

  short_name = options.use_proxy_hash or options.enable_jni_multiplexing
  sub_dict = {
      'ESCAPED_PROXY_CLASS':
      common.escape_class_name(gen_jni_class.full_name_with_slashes),
      'PROXY_CLASS':
      gen_jni_class.full_name_with_slashes,
      'KMETHODS':
      registration_dict['PROXY_NATIVE_METHOD_ARRAY'],
      'REGISTRATION_NAME':
      jni_generator.GetRegistrationFunctionName(
          gen_jni_class.full_name_with_slashes),
  }

  if registration_dict['PROXY_NATIVE_METHOD_ARRAY']:
    proxy_native_array = registration_template.substitute(sub_dict)
    proxy_natives_registration = registration_call.substitute(sub_dict)
  else:
    proxy_native_array = ''
    proxy_natives_registration = ''

  registration_dict['PROXY_NATIVE_METHOD_ARRAY'] = proxy_native_array
  registration_dict['REGISTER_PROXY_NATIVES'] = proxy_natives_registration

  if options.manual_jni_registration:
    registration_dict['MANUAL_REGISTRATION'] = manual_registration.substitute(
        registration_dict)
  else:
    registration_dict['MANUAL_REGISTRATION'] = ''


def CreateProxyJavaFromDict(options,
                            gen_jni_class,
                            registration_dict,
                            stub_methods='',
                            forwarding=False):
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
    fields = ''

  if forwarding:
    methods = registration_dict['FORWARDING_PROXY_METHODS']
  else:
    methods = registration_dict['PROXY_NATIVE_SIGNATURES']
  methods += stub_methods

  return template.substitute({
      'CLASS_NAME': gen_jni_class.name,
      'FIELDS': fields,
      'PACKAGE': gen_jni_class.package_with_dots,
      'METHODS': methods
  })


def CreateFromDict(options, registration_dict):
  """Returns the content of the header file."""

  template = string.Template("""\
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     third_party/jni_zero/jni_registration_generator.py
// Please do not change its content.

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

#include <iterator>

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_int_wrapper.h"


// Step 1: Forward declarations (classes).
${CLASS_PATH_DECLARATIONS}

// Step 2: Forward declarations (methods).

${FORWARD_DECLARATIONS}
${FORWARDING_CALLS}
${MANUAL_REGISTRATION}
#endif  // ${HEADER_GUARD}
""")
  gen_jni_class = proxy.get_gen_jni_class(short=options.use_proxy_hash
                                          or options.enable_jni_multiplexing,
                                          name_prefix=options.module_name,
                                          package_prefix=options.package_prefix)
  _SetProxyRegistrationFields(options, gen_jni_class, registration_dict)
  if not options.enable_jni_multiplexing:
    registration_dict['FORWARDING_CALLS'] = ''
  if len(registration_dict['FORWARD_DECLARATIONS']) == 0:
    return ''

  return template.substitute(registration_dict)


def _GetJavaToNativeParamsList(param_types):
  if not param_types:
    return 'jlong switch_num'

  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  params_in_stub = []
  for t in param_types:
    params_type_count[t] += 1
    params_in_stub.append('%s %s_param%d' % (t.to_cpp(), t.to_java().replace(
        '[]', '_array').lower(), params_type_count[t]))

  return 'jlong switch_num, ' + ', '.join(params_in_stub)


class DictionaryGenerator(object):
  """Generates an inline header file for JNI registration."""
  def __init__(self, jni_obj, options):
    self.options = options
    self.file_path = jni_obj.filename
    self.content_namespace = jni_obj.jni_namespace
    self.natives = jni_obj.natives
    self.proxy_natives = jni_obj.proxy_natives
    self.non_proxy_natives = jni_obj.non_proxy_natives
    self.fully_qualified_class = jni_obj.java_class.full_name_with_slashes
    self.type_resolver = jni_obj.type_resolver
    self.class_name = jni_obj.java_class.name
    self.helper = jni_generator.HeaderFileGeneratorHelper(
        jni_obj.java_class,
        module_name=jni_obj.module_name,
        use_proxy_hash=options.use_proxy_hash,
        package_prefix=options.package_prefix,
        enable_jni_multiplexing=options.enable_jni_multiplexing)
    self.registration_dict = None
    self.gen_jni_class = proxy.get_gen_jni_class(
        short=options.use_proxy_hash or options.enable_jni_multiplexing,
        name_prefix=options.module_name,
        package_prefix=options.package_prefix)

  def Generate(self):
    self.registration_dict = {
        'FULL_CLASS_NAME': self.fully_qualified_class,
        'FILE_PATH': self.file_path,
    }
    self._AddClassPathDeclarations()
    self._AddForwardDeclaration()
    self._AddJNINativeMethodsArrays()
    self._AddProxyNativeMethodKStrings()
    self._AddRegisterNativesCalls()
    self._AddRegisterNativesFunctions()

    self.registration_dict['PROXY_NATIVE_SIGNATURES'] = (''.join(
        _MakeProxySignature(self.options, native)
        for native in self.proxy_natives))

    if self.options.enable_jni_multiplexing:
      self._AssignSwitchNumberToNatives()
      self._AddCases()

    if self.options.use_proxy_hash or self.options.enable_jni_multiplexing:
      self.registration_dict['FORWARDING_PROXY_METHODS'] = ('\n'.join(
          _MakeForwardingProxy(self.options, self.gen_jni_class, native)
          for native in self.proxy_natives))

    return self.registration_dict

  def _SetDictValue(self, key, value):
    self.registration_dict[key] = jni_generator.WrapOutput(value)

  def _AddClassPathDeclarations(self):
    classes = self.helper.GetUniqueClasses(self.natives)
    self._SetDictValue(
        'CLASS_PATH_DECLARATIONS',
        self.helper.GetClassPathLines(classes, declare_only=True))

  def _AddForwardDeclaration(self):
    """Add the content of the forward declaration to the dictionary."""
    template = string.Template("""\
JNI_GENERATOR_EXPORT ${RETURN} ${STUB_NAME}(
    JNIEnv* env,
    ${PARAMS_IN_STUB});
""")
    forward_declaration = ''
    for native in self.natives:
      value = {
          'RETURN': native.proxy_return_type.to_cpp(),
          'STUB_NAME': self.helper.GetStubName(native),
          'PARAMS_IN_STUB': jni_generator.GetParamsInStub(native),
      }
      forward_declaration += template.substitute(value)
    self._SetDictValue('FORWARD_DECLARATIONS', forward_declaration)

  def _AddRegisterNativesCalls(self):
    """Add the body of the RegisterNativesImpl method to the dictionary."""

    # Only register if there is at least 1 non-proxy native
    if len(self.non_proxy_natives) == 0:
      return ''

    template = string.Template("""\
  if (!${REGISTER_NAME}(env))
    return false;
""")
    value = {
        'REGISTER_NAME':
        jni_generator.GetRegistrationFunctionName(self.fully_qualified_class)
    }
    register_body = template.substitute(value)
    self._SetDictValue('REGISTER_NON_NATIVES', register_body)

  def _AddJNINativeMethodsArrays(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}
};

""")
    open_namespace = ''
    close_namespace = ''
    if self.content_namespace:
      parts = self.content_namespace.split('::')
      all_namespaces = ['namespace %s {' % ns for ns in parts]
      open_namespace = '\n'.join(all_namespaces) + '\n'
      all_namespaces = ['}  // namespace %s' % ns for ns in parts]
      all_namespaces.reverse()
      close_namespace = '\n'.join(all_namespaces) + '\n\n'

    body = self._SubstituteNativeMethods(template)
    if body:
      self._SetDictValue('JNI_NATIVE_METHOD_ARRAY', ''.join(
          (open_namespace, body, close_namespace)))

  def _GetKMethodsString(self, clazz):
    if clazz != self.class_name:
      return ''
    ret = [self._GetKMethodArrayEntry(n) for n in self.non_proxy_natives]
    return '\n'.join(ret)

  def _GetKMethodArrayEntry(self, native):
    template = string.Template('    { "${NAME}", "${JNI_DESCRIPTOR}", ' +
                               'reinterpret_cast<void*>(${STUB_NAME}) },')

    name = 'native' + native.cpp_name
    jni_descriptor = native.proxy_signature.to_descriptor()
    stub_name = self.helper.GetStubName(native)

    if native.is_proxy:
      # Literal name of the native method in the class that contains the actual
      # native declaration.
      if self.options.enable_jni_multiplexing:
        class_name = common.escape_class_name(
            self.gen_jni_class.full_name_with_slashes)
        name = _GetMultiplexProxyName(native.proxy_signature)
        proxy_signature = common.escape_class_name(name)
        stub_name = 'Java_' + class_name + '_' + proxy_signature

        multipliexed_signature = java_types.JavaSignature(
            native.return_type, (java_types.LONG, ), None)
        jni_descriptor = multipliexed_signature.to_descriptor()
      elif self.options.use_proxy_hash:
        name = native.hashed_proxy_name
      else:
        name = native.proxy_name
    values = {
        'NAME': name,
        'JNI_DESCRIPTOR': jni_descriptor,
        'STUB_NAME': stub_name
    }
    return template.substitute(values)

  def _AddProxyNativeMethodKStrings(self):
    """Returns KMethodString for wrapped native methods in all_classes """

    proxy_k_strings = ('\n'.join(
        self._GetKMethodArrayEntry(p) for p in self.proxy_natives))

    self._SetDictValue('PROXY_NATIVE_METHOD_ARRAY', proxy_k_strings)

  def _SubstituteNativeMethods(self, template):
    """Substitutes NAMESPACE, JAVA_CLASS and KMETHODS in the provided
    template."""
    ret = []
    all_classes = self.helper.GetUniqueClasses(self.natives)
    all_classes[self.class_name] = self.fully_qualified_class

    for clazz, full_clazz in all_classes.items():
      if clazz == self.gen_jni_class.name:
        continue

      kmethods = self._GetKMethodsString(clazz)
      namespace_str = ''
      if self.content_namespace:
        namespace_str = self.content_namespace + '::'
      if kmethods:
        values = {
            'NAMESPACE': namespace_str,
            'JAVA_CLASS': common.escape_class_name(full_clazz),
            'KMETHODS': kmethods
        }
        ret += [template.substitute(values)]
    if not ret: return ''
    return '\n'.join(ret)

  def GetJNINativeMethodsString(self):
    """Returns the implementation of the array of native methods."""
    template = string.Template("""\
static const JNINativeMethod kMethods_${JAVA_CLASS}[] = {
${KMETHODS}

};
""")
    return self._SubstituteNativeMethods(template)

  def _AddRegisterNativesFunctions(self):
    """Returns the code for RegisterNatives."""
    natives = self._GetRegisterNativesImplString()
    if not natives:
      return ''
    template = string.Template("""\
JNI_REGISTRATION_EXPORT bool ${REGISTER_NAME}(JNIEnv* env) {
${NATIVES}\
  return true;
}

""")
    values = {
        'REGISTER_NAME':
        jni_generator.GetRegistrationFunctionName(self.fully_qualified_class),
        'NATIVES':
        natives
    }
    self._SetDictValue('JNI_NATIVE_METHOD', template.substitute(values))

  def _GetRegisterNativesImplString(self):
    """Returns the shared implementation for RegisterNatives."""
    template = string.Template("""\
  const int kMethods_${JAVA_CLASS}Size =
      std::size(${NAMESPACE}kMethods_${JAVA_CLASS});
  if (env->RegisterNatives(
      ${JAVA_CLASS}_clazz(env),
      ${NAMESPACE}kMethods_${JAVA_CLASS},
      kMethods_${JAVA_CLASS}Size) < 0) {
    jni_generator::HandleRegistrationError(env,
        ${JAVA_CLASS}_clazz(env),
        __FILE__);
    return false;
  }

""")
    # Only register if there is a native method not in a proxy,
    # since all the proxies will be registered together.
    if len(self.non_proxy_natives) != 0:
      return self._SubstituteNativeMethods(template)
    return ''

  def _AssignSwitchNumberToNatives(self):
    # The switch number for a native method is a 64-bit long with the first
    # bit being a sign digit. The signed two's complement is taken when
    # appropriate to make use of negative numbers.
    for native in self.proxy_natives:
      hashed_long = hashlib.md5(
          native.proxy_name.encode('utf-8')).hexdigest()[:16]
      switch_num = int(hashed_long, 16)
      if (switch_num & 1 << 63):
        switch_num -= (1 << 64)

      native.switch_num = str(switch_num)

  def _AddCases(self):
    # Switch cases are grouped together by the same proxy signatures.
    template = string.Template("""
          case ${SWITCH_NUM}:
            return ${STUB_NAME}(env, jcaller${PARAMS});
          """)

    signature_to_cases = collections.defaultdict(list)
    for native in self.proxy_natives:
      signature = native.proxy_signature
      params = _GetParamsListForMultiplex(native.proxy_params, with_types=False)
      values = {
          'SWITCH_NUM': native.switch_num,
          # We are forced to call the generated stub instead of the impl because
          # the impl is not guaranteed to have a globally unique name.
          'STUB_NAME': self.helper.GetStubName(native),
          'PARAMS': params,
      }
      signature_to_cases[signature].append(template.substitute(values))

    self.registration_dict['SIGNATURE_TO_CASES'] = signature_to_cases


def _GetParamsListForMultiplex(params, *, with_types):
  if not params:
    return ''

  # Parameters are named after their type, with a unique number per parameter
  # type to make sure the names are unique, even within the same types.
  params_type_count = collections.defaultdict(int)
  sb = []
  for p in params:
    type_str = p.java_type.to_java()
    params_type_count[type_str] += 1
    param_type = f'{type_str} ' if with_types else ''
    sb.append('%s%s_param%d' % (param_type, type_str.replace(
        '[]', '_array').lower(), params_type_count[type_str]))

  return ', ' + ', '.join(sb)


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
    'boolean': 'Z',
}


def _GetMultiplexProxyName(signature):
  # Proxy signatures for methods are named after their return type and
  # parameters to ensure uniqueness, even for the same return types.
  params_part = ''
  params_list = [t.to_java() for t in signature.param_types]
  # Parameter types could contain multi-dimensional arrays and every
  # instance of [] has to be replaced in the proxy signature name.
  for k, v in _MULTIPLEXED_CHAR_BY_TYPE.items():
    params_list = [p.replace(k, v) for p in params_list]
  params_part = ''
  if params_list:
    params_part = '_' + ''.join(p for p in params_list)

  java_return_type = signature.return_type.to_java()
  return_value_part = java_return_type.replace('[]', '_array').lower()
  return 'resolve_for_' + return_value_part + params_part


def _MakeForwardingProxy(options, gen_jni_class, proxy_native):
  template = string.Template("""
    public static ${RETURN_TYPE} ${METHOD_NAME}(${PARAMS_WITH_TYPES}) {
        ${MAYBE_RETURN}${PROXY_CLASS}.${PROXY_METHOD_NAME}(${PARAM_NAMES});
    }""")

  param_names = proxy_native.proxy_params.to_call_str()

  if options.enable_jni_multiplexing:
    if not param_names:
      param_names = proxy_native.switch_num + 'L'
    else:
      param_names = proxy_native.switch_num + 'L, ' + param_names
    proxy_method_name = _GetMultiplexProxyName(proxy_native.proxy_signature)
  else:
    proxy_method_name = proxy_native.hashed_proxy_name

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
    public static native ${RETURN} ${PROXY_NAME}(${PARAMS_WITH_TYPES});"""

  if options.enable_jni_multiplexing:
    # This has to be only one line and without comments because all the proxy
    # signatures will be joined, then split on new lines with duplicates removed
    # since multiple |proxy_native|s map to the same multiplexed signature.
    signature_template = string.Template(native_method_line)

    alt_name = None
    proxy_name = _GetMultiplexProxyName(proxy_native.proxy_signature)
    params_with_types = 'long switch_num' + _GetParamsListForMultiplex(
        proxy_native.proxy_params, with_types=True)
  elif options.use_proxy_hash:
    signature_template = string.Template("""
      // Original name: ${ALT_NAME}""" + native_method_line)

    alt_name = proxy_native.proxy_name
    proxy_name = proxy_native.hashed_proxy_name
  else:
    signature_template = string.Template("""
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


def main(parser, args):
  if not args.enable_proxy_mocks and args.require_mocks:
    parser.error('--require-mocks requires --enable-proxy-mocks.')
  if not args.header_path and args.manual_jni_registration:
    parser.error('--manual-jni-registration requires --header-path.')
  if args.remove_uncalled_methods and not args.native_sources_file:
    parser.error('--remove-uncalled-methods requires --native-sources-file.')

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

  _Generate(args, native_sources, java_sources=java_sources)

  if args.depfile:
    # GN does not declare a dep on the sources files to avoid circular
    # dependencies, so they need to be listed here.
    all_inputs = native_sources + java_sources + [args.java_sources_file]
    if args.native_sources_file:
      all_inputs.append(args.native_sources_file)
    action_helpers.write_depfile(args.depfile, args.srcjar_path, all_inputs)
