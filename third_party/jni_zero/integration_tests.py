#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for jni_zero.py.

This test suite contains various tests for the JNI generator.
It exercises the low-level parser all the way up to the
code generator and ensures the output matches a golden
file.
"""

import collections
import copy
import difflib
import glob
import logging
import os
import pathlib
import shlex
import subprocess
import sys
import tempfile
import unittest
import zipfile

_SCRIPT_DIR = os.path.normpath(os.path.dirname(__file__))
_GOLDENS_DIR = os.path.join(_SCRIPT_DIR, 'golden')
_EXTRA_INCLUDES = 'third_party/jni_zero/jni_zero_helper.h'
_JAVA_SRC_DIR = os.path.join(_SCRIPT_DIR, 'samples', 'java', 'src', 'org',
                             'jni_zero', 'samples')

# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE = os.environ.get('REBASELINE', '0') != '0'

_accessed_goldens = set()


class CliOptions:
  def __init__(self, is_final=False, is_javap=False, **kwargs):
    if is_final:
      self.action = 'generate-final'
    elif is_javap:
      self.action = 'from-jar'
    else:
      self.action = 'from-source'

    self.input_file = None
    self.jar_file = None
    self.output_dir = None
    self.output_name = None if is_final else 'output.h'
    self.header_path = None
    self.enable_jni_multiplexing = False
    self.package_prefix = None
    self.use_proxy_hash = False
    self.extra_include = None if is_final else _EXTRA_INCLUDES
    self.module_name = None
    self.add_stubs_for_missing_native = False
    self.enable_proxy_mocks = False
    self.include_test_only = False
    self.manual_jni_registration = False
    self.remove_uncalled_methods = False
    self.require_mocks = False
    self.__dict__.update(kwargs)

  def to_args(self):
    ret = [os.path.join(_SCRIPT_DIR, 'jni_zero.py'), self.action]
    if self.enable_jni_multiplexing:
      ret.append('--enable-jni-multiplexing')
    if self.package_prefix:
      ret += ['--package-prefix', self.package_prefix]
    if self.use_proxy_hash:
      ret.append('--use-proxy-hash')
    if self.output_dir:
      ret += ['--output-dir', self.output_dir]
    if self.input_file:
      ret += ['--input-file', self.input_file]
    if self.output_name:
      ret += ['--output-name', self.output_name]
    if self.jar_file:
      ret += ['--jar-file', self.jar_file]
    if self.extra_include:
      ret += ['--extra-include', self.extra_include]
    if self.add_stubs_for_missing_native:
      ret.append('--add-stubs-for-missing-native')
    if self.enable_proxy_mocks:
      ret.append('--enable-proxy-mocks')
    if self.header_path:
      ret += ['--header-path', self.header_path]
    if self.include_test_only:
      ret.append('--include-test-only')
    if self.manual_jni_registration:
      ret.append('--manual-jni-registration')
    if self.module_name:
      ret += ['--module-name', self.module_name]
    if self.remove_uncalled_methods:
      ret.append('--remove-uncalled-methods')
    if self.require_mocks:
      ret.append('--require-mocks')
    return ret


def _MakePrefixes(options):
  package_prefix = ''
  if options.package_prefix:
    package_prefix = options.package_prefix.replace('.', '/') + '/'
  module_prefix = ''
  if options.module_name:
    module_prefix = f'{options.module_name}_'
  return package_prefix, module_prefix


class BaseTest(unittest.TestCase):
  def _CheckSrcjarGoldens(self, srcjar_path, name_to_goldens):
    with zipfile.ZipFile(srcjar_path, 'r') as srcjar:
      self.assertEqual(set(srcjar.namelist()), set(name_to_goldens))
      for name in srcjar.namelist():
        self.assertTrue(
            name in name_to_goldens,
            f'Found {name} output, but not present in name_to_goldens map.')
        contents = srcjar.read(name).decode('utf-8')
        self.AssertGoldenTextEquals(contents, name_to_goldens[name])

  def _TestEndToEndGeneration(self, input_file, *, srcjar=False, **kwargs):
    is_javap = input_file.endswith('.class')
    golden_name = self._testMethodName
    options = CliOptions(is_javap=is_javap, **kwargs)
    basename = os.path.splitext(input_file)[0]
    header_golden = f'{golden_name}-{basename}_jni.h.golden'
    if srcjar:
      dir_prefix, file_prefix = _MakePrefixes(options)
      name_to_goldens = {
          f'{dir_prefix}org/jni_zero/{file_prefix}GEN_JNI.java':
          f'{golden_name}-Placeholder-GEN_JNI.java.golden',
          f'org/jni_zero/samples/{basename}Jni.java':
          f'{golden_name}-{basename}Jni.java.golden',
      }

    with tempfile.TemporaryDirectory() as tdir:
      relative_input_file = os.path.join(_JAVA_SRC_DIR, input_file)
      if is_javap:
        jar_path = os.path.join(tdir, 'input.jar')
        with zipfile.ZipFile(jar_path, 'w') as z:
          z.write(relative_input_file, input_file)
        options.jar_file = jar_path
        options.input_file = input_file
      else:
        options.input_file = relative_input_file

      options.output_dir = tdir
      cmd = options.to_args()
      if srcjar:
        srcjar_path = os.path.join(tdir, 'srcjar.jar')
        cmd += ['--srcjar-path', srcjar_path]

      logging.info('Running: %s', shlex.join(cmd))
      subprocess.check_call(cmd)

      output_path = os.path.join(tdir, options.output_name)
      with open(output_path, 'r') as f:
        contents = f.read()
      self.AssertGoldenTextEquals(contents, header_golden)

      if srcjar:
        self._CheckSrcjarGoldens(srcjar_path, name_to_goldens)

  def _TestEndToEndRegistration(self,
                                input_files,
                                src_files_for_asserts_and_stubs=None,
                                **kwargs):
    golden_name = self._testMethodName
    options = CliOptions(is_final=True, **kwargs)
    dir_prefix, file_prefix = _MakePrefixes(options)
    name_to_goldens = {
        f'{dir_prefix}org/jni_zero/{file_prefix}GEN_JNI.java':
        f'{golden_name}-Final-GEN_JNI.java.golden',
    }
    if options.use_proxy_hash:
      name_to_goldens[f'{dir_prefix}J/{file_prefix}N.java'] = (
          f'{golden_name}-Final-N.java.golden')
    header_golden = None
    if options.use_proxy_hash or options.manual_jni_registration:
      header_golden = f'{golden_name}-Registration.h.golden'

    with tempfile.TemporaryDirectory() as tdir:
      native_sources = [os.path.join(_JAVA_SRC_DIR, f) for f in input_files]

      if src_files_for_asserts_and_stubs:
        java_sources = [
            os.path.join(_JAVA_SRC_DIR, f)
            for f in src_files_for_asserts_and_stubs
        ]
      else:
        java_sources = native_sources

      cmd = options.to_args()

      java_sources_file = pathlib.Path(tdir) / 'java_sources.txt'
      java_sources_file.write_text('\n'.join(java_sources))
      cmd += ['--java-sources-file', str(java_sources_file)]
      if native_sources:
        native_sources_file = pathlib.Path(tdir) / 'native_sources.txt'
        native_sources_file.write_text('\n'.join(native_sources))
        cmd += ['--native-sources-file', str(native_sources_file)]

      srcjar_path = os.path.join(tdir, 'srcjar.jar')
      cmd += ['--srcjar-path', srcjar_path]
      if header_golden:
        header_path = os.path.join(tdir, 'header.h')
        cmd += ['--header-path', header_path]

      logging.info('Running: %s', shlex.join(cmd))
      subprocess.check_call(cmd)

      self._CheckSrcjarGoldens(srcjar_path, name_to_goldens)

      if header_golden:
        with open(header_path, 'r') as f:
          # Temp directory will cause some diffs each time we run if we don't
          # normalize.
          contents = f.read().replace(
              tdir.replace('/', '_').upper(), 'TEMP_DIR')
          self.AssertGoldenTextEquals(contents, header_golden)

  def _TestParseError(self, error_snippet, input_data):
    with tempfile.TemporaryDirectory() as tdir:
      input_file = os.path.join(tdir, 'MyFile.java')
      pathlib.Path(input_file).write_text(input_data)
      options = CliOptions()
      options.input_file = input_file
      options.output_dir = tdir
      cmd = options.to_args()

      logging.info('Running: %s', shlex.join(cmd))
      result = subprocess.run(cmd, capture_output=True, check=False, text=True)
      self.assertEqual(result.returncode, 1)
      self.assertIn('MyFile.java', result.stderr)
      self.assertIn(error_snippet, result.stderr)
      return result.stderr

  def _ReadGoldenFile(self, path):
    _accessed_goldens.add(path)
    if not os.path.exists(path):
      return None
    with open(path, 'r') as f:
      return f.read()

  def AssertTextEquals(self, golden_text, generated_text):
    if not self.CompareText(golden_text, generated_text):
      self.fail('Golden text mismatch.')

  def CompareText(self, golden_text, generated_text):
    def FilterText(text):
      return [
          l.strip() for l in text.split('\n')
          if not l.startswith('// Copyright')
      ]

    stripped_golden = FilterText(golden_text)
    stripped_generated = FilterText(generated_text)
    if stripped_golden == stripped_generated:
      return True
    print(self.id())
    for line in difflib.context_diff(stripped_golden, stripped_generated):
      print(line)
    print('\n\nGenerated')
    print('=' * 80)
    print(generated_text)
    print('=' * 80)
    print('Run with:')
    print('REBASELINE=1', sys.argv[0])
    print('to regenerate the data files.')

  def AssertGoldenTextEquals(self, generated_text, golden_file):
    """Compares generated text with the corresponding golden_file

    It will instead compare the generated text with
    script_dir/golden/golden_file."""
    golden_path = os.path.join(_GOLDENS_DIR, golden_file)
    golden_text = self._ReadGoldenFile(golden_path)
    if _REBASELINE:
      if golden_text != generated_text:
        print('Updated', golden_path)
        with open(golden_path, 'w') as f:
          f.write(generated_text)
      return
    # golden_text is None if no file is found. Better to fail than in
    # AssertTextEquals so we can give a clearer message.
    if golden_text is None:
      self.fail('Golden file does not exist: ' + golden_path)
    self.AssertTextEquals(golden_text, generated_text)


@unittest.skipIf(os.name == 'nt', 'Not intended to work on Windows')
class Tests(BaseTest):
  def testNonProxy(self):
    self._TestEndToEndGeneration('SampleNonProxy.java')

  def testBirectionalNonProxy(self):
    self._TestEndToEndGeneration('SampleBidirectionalNonProxy.java')

  def testBidirectionalClass(self):
    self._TestEndToEndGeneration('SampleForTests.java', srcjar=True)
    self._TestEndToEndRegistration(['SampleForTests.java'])

  def testFromClassFile(self):
    self._TestEndToEndGeneration('JavapClass.class')

  def testUniqueAnnotations(self):
    self._TestEndToEndGeneration('SampleUniqueAnnotations.java', srcjar=True)

  def testEndToEndProxyHashed(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   use_proxy_hash=True)

  def testEndToEndManualRegistration(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   manual_jni_registration=True)

  def testEndToEndManualRegistration_NonProxy(self):
    self._TestEndToEndRegistration(['SampleNonProxy.java'],
                                   manual_jni_registration=True)

  def testEndToEndProxyJniWithModules(self):
    self._TestEndToEndGeneration('SampleModule.java',
                                 srcjar=True,
                                 use_proxy_hash=True,
                                 module_name='module')
    self._TestEndToEndRegistration(
        ['SampleForAnnotationProcessor.java', 'SampleModule.java'],
        use_proxy_hash=True,
        module_name='module')

  def testStubRegistration(self):
    input_java_files = ['SampleForAnnotationProcessor.java']
    stubs_java_files = input_java_files + [
        'TinySample.java', 'SampleProxyEdgeCases.java'
    ]
    extra_input_java_files = ['TinySample2.java']
    self._TestEndToEndRegistration(
        input_java_files + extra_input_java_files,
        src_files_for_asserts_and_stubs=stubs_java_files,
        add_stubs_for_missing_native=True,
        remove_uncalled_methods=True)

  def testFullStubs(self):
    self._TestEndToEndRegistration(
        [],
        src_files_for_asserts_and_stubs=['TinySample.java'],
        add_stubs_for_missing_native=True)

  def testForTestingKept(self):
    input_java_file = 'SampleProxyEdgeCases.java'
    self._TestEndToEndGeneration(input_java_file, srcjar=True)
    self._TestEndToEndRegistration([input_java_file],
                                   use_proxy_hash=True,
                                   include_test_only=True)

  def testForTestingRemoved(self):
    self._TestEndToEndRegistration(['SampleProxyEdgeCases.java'],
                                   use_proxy_hash=True,
                                   include_test_only=True)

  def testProxyMocks(self):
    self._TestEndToEndRegistration(['TinySample.java'], enable_proxy_mocks=True)

  def testRequireProxyMocks(self):
    self._TestEndToEndRegistration(['TinySample.java'],
                                   enable_proxy_mocks=True,
                                   require_mocks=True)

  def testPackagePrefixGenerator(self):
    self._TestEndToEndGeneration('SampleForTests.java',
                                 srcjar=True,
                                 package_prefix='this.is.a.package.prefix')

  def testPackagePrefixWithManualRegistration(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   package_prefix='this.is.a.package.prefix',
                                   manual_jni_registration=True)

  def testPackagePrefixWithProxyHash(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   package_prefix='this.is.a.package.prefix',
                                   use_proxy_hash=True)

  def testPackagePrefixWithManualRegistrationWithProxyHash(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   package_prefix='this.is.a.package.prefix',
                                   use_proxy_hash=True,
                                   manual_jni_registration=True)

  def testMultiplexing(self):
    self._TestEndToEndRegistration(['SampleForAnnotationProcessor.java'],
                                   enable_jni_multiplexing=True,
                                   use_proxy_hash=True)

  def testParseError_noPackage(self):
    data = """
class MyFile {}
"""
    self._TestParseError('Unable to find "package" line', data)

  def testParseError_noClass(self):
    data = """
package foo;
"""
    self._TestParseError('No classes found', data)

  def testParseError_wrongClass(self):
    data = """
package foo;
class YourFile {}
"""
    self._TestParseError('Found class "YourFile" but expected "MyFile"', data)

  def testParseError_noMethods(self):
    data = """
package foo;
class MyFile {
  void foo() {}
}
"""
    self._TestParseError('No native methods found', data)

  def testParseError_noInterfaceMethods(self):
    data = """
package foo;
class MyFile {
  @NativeMethods
  interface A {}
}
"""
    self._TestParseError('Found no methods within', data)

  def testParseError_twoInterfaces(self):
    data = """
package foo;
class MyFile {
  @NativeMethods
  interface A {
    void a();
  }
  @NativeMethods
  interface B {
    void b();
  }
}
"""
    self._TestParseError('Multiple @NativeMethod interfaces', data)

  def testParseError_twoNamespaces(self):
    data = """
package foo;
@JNINamespace("one")
@JNINamespace("two")
class MyFile {
  @NativeMethods
  interface A {
    void a();
  }
}
"""
    self._TestParseError('Found multiple @JNINamespace', data)


def main():
  try:
    unittest.main()
  finally:
    if _REBASELINE and not any(not x.startswith('-') for x in sys.argv[1:]):
      for path in glob.glob(os.path.join(_GOLDENS_DIR, '*.golden')):
        if path not in _accessed_goldens:
          print('Removing obsolete golden:', path)
          os.unlink(path)


if __name__ == '__main__':
  main()
