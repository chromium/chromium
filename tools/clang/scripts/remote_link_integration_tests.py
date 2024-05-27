#! /usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Integration tests for remote_link.
#
# Usage:
#
# Ensure that rewrapper, llvm-objdump, and llvm-dwarfdump are in your
# PATH.
# Then run:
#
#   tools/clang/scripts/remote_link_integration_tests.py
#
# See also remote_link_unit_tests.py, which contains unit tests and
# instructions for generating coverage information.

import remote_ld
import remote_link

from io import StringIO
import os
import re
import shlex
import subprocess
import unittest
from unittest import mock

from remote_link_test_utils import named_directory, working_directory

# Path constants.
CHROMIUM_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
LLVM_BIN_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                            'Release+Asserts', 'bin')


def _create_inputs(path):
  """
  Creates input files under path.
  """
  with open(os.path.join(path, 'main.cpp'), 'w') as f:
    f.write('extern int foo();\n'
            'int main(int argc, char *argv[]) {\n  return foo();\n}\n')
  with open(os.path.join(path, 'foo.cpp'), 'w') as f:
    f.write('int foo() {\n  return 12;\n}\n')
  with open(os.path.join(path, 'bar.cpp'), 'w') as f:
    f.write('int bar() {\n  return 9;\n}\n')


def _lto_args(generate_bitcode):
  """
  Returns list of arguments to clang to generate bitcode or not.
  """
  if generate_bitcode:
    return ['-flto=thin']
  else:
    return []


class RemoteLinkUnixAllowMain(remote_ld.RemoteLinkUnix):
  """
  Same as remote_ld.RemoteLinkUnix, but has "main" on the allow list.
  """

  def __init__(self, *args, **kwargs):
    super(RemoteLinkUnixAllowMain, self).__init__(*args, **kwargs)
    self.ALLOWLIST = {'main'}


class RemoteLinkWindowsAllowMain(remote_link.RemoteLinkWindows):
  """
  Same as remote_ld.RemoteLinkWindows, but has "main" on the allow list.
  """

  def __init__(self, *args, **kwargs):
    super(RemoteLinkWindowsAllowMain, self).__init__(*args, **kwargs)
    self.ALLOWLIST = {'main.exe'}


class RemoteLinkIntegrationTest(unittest.TestCase):
  def clangcl(self):
    return os.path.join(LLVM_BIN_DIR, 'clang-cl' + remote_link.exe_suffix())

  def lld_link(self):
    return os.path.join(LLVM_BIN_DIR, 'lld-link' + remote_link.exe_suffix())

  def llvmar(self):
    return os.path.join(LLVM_BIN_DIR, 'llvm-ar' + remote_link.exe_suffix())

  def test_distributed_lto_common_objs(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'main.cpp',
          '-Foobj/main.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-Foobj/foo.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', 'bar.cpp', '-Foobj/bar.obj'
      ])
      subprocess.check_call([
          self.llvmar(), 'crsT', 'obj/foobar.lib', 'obj/bar.obj', 'obj/foo.obj'
      ])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.obj\n' 'obj/foobar.lib\n')
      with open('my_reclient.sh', 'w') as f:
        f.write('#! /bin/sh\n\nrewrapper "$@"\n')
      os.chmod('my_reclient.sh', 0o755)
      rc = remote_link.RemoteLinkWindows().main([
          'remote_link.py', '--wrapper', './my_reclient.sh', '--ar-path',
          self.llvmar(), '--',
          self.lld_link(), '-nodefaultlib', '-entry:main', '-out:main.exe',
          '@main.rsp'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main.exe', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('my_reclient.sh', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build common_objs/obj/main.obj.stamp : codegen ',
                      buildrules)
        self.assertIn('build common_objs/obj/foo.obj.stamp : codegen ',
                      buildrules)
        self.assertIn(' index = common_objs/empty.thinlto.bc', buildrules)
        link_match = re.search('^build main.exe : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertNotIn('main.exe.split.obj', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main.exe'])
      # There are no symbols in the disassembly, but we're expecting two
      # functions, one of which calls the other.
      self.assertTrue(b'call' in disasm or b'jmp' in disasm)

  def test_distributed_lto_allowlist(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'main.cpp',
          '-Foobj/main.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'foo.cpp',
          '-Foobj/foo.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-Os', '-flto=thin', '-m32', 'bar.cpp',
          '-Foobj/bar.obj'
      ])
      subprocess.check_call([
          self.llvmar(), 'crsT', 'obj/foobar.lib', 'obj/bar.obj', 'obj/foo.obj'
      ])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.obj\n' 'obj/foobar.lib\n')
      rc = RemoteLinkWindowsAllowMain().main([
          'remote_link.py', '--wrapper', 'rewrapper', '--ar-path',
          self.llvmar(), '--',
          self.lld_link(), '-nodefaultlib', '-entry:main', '-machine:X86',
          '-opt:lldlto=2', '-mllvm:-import-instr-limit=10', '-out:main.exe',
          '@main.rsp'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main.exe', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('rewrapper', codegen_text)
        self.assertIn('-m32', codegen_text)
        self.assertIn('-mllvm -import-instr-limit=10', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build lto.main.exe/obj/main.obj.stamp : codegen ',
                      buildrules)
        self.assertIn('build lto.main.exe/obj/foo.obj.stamp : codegen ',
                      buildrules)
        link_match = re.search('^build main.exe : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertIn('main.exe.split.obj', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main.exe'])
      # There are no symbols in the disassembly, but we're expecting a single
      # function, with no calls or jmps.
      self.assertNotIn(b'jmp', disasm)
      self.assertNotIn(b'call', disasm)

  def test_override_allowlist(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangcl(), '-c', '-O2', '-flto=thin', 'main.cpp',
          '-Foobj/main.obj'
      ])
      subprocess.check_call([
          self.clangcl(), '-c', '-O2', '-flto=thin', 'foo.cpp', '-Foobj/foo.obj'
      ])
      rc = remote_link.RemoteLinkWindows().main([
          'remote_link.py', '--generate', '--allowlist', '--ar-path',
          self.llvmar(), '--',
          self.lld_link(), '-nodefaultlib', '-entry:main', '-opt:lldlto=2',
          '-out:main.exe', 'obj/main.obj', 'obj/foo.obj'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check that we have rules for main and foo, and that they are
      # not common objects.
      with open(os.path.join(d, 'lto.main.exe', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search(r'^rule codegen\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build lto.main.exe/obj/main.obj.stamp : codegen ',
                      buildrules)
        self.assertIn('build lto.main.exe/obj/foo.obj.stamp : codegen ',
                      buildrules)
        link_match = re.search(r'^build main.exe : native-link\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)


class RemoteLdIntegrationTest(unittest.TestCase):
  def clangxx(self):
    return os.path.join(LLVM_BIN_DIR, 'clang++' + remote_link.exe_suffix())

  def llvmar(self):
    return os.path.join(LLVM_BIN_DIR, 'llvm-ar' + remote_link.exe_suffix())

  def test_nonlto(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'main.cpp', '-o', 'main.o'])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'foo.cpp', '-o', 'foo.o'])
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--wrapper', 'rewrapper', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', 'main.o', 'foo.o', '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # lto.main directory should not be present.
      self.assertFalse(os.path.exists(os.path.join(d, 'lto.main')))
      # Check that main calls foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertIn(b'foo', main_disasm)

  def test_fallback_lto(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = remote_ld.RemoteLinkUnix().main([
          'remote_ld.py', '--wrapper', 'rewrapper', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # lto.main directory should not be present.
      self.assertFalse(os.path.exists(os.path.join(d, 'lto.main')))
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '-j', '16', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have rewrapper invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertIn('rewrapper ', buildrules)
        self.assertIn('build lto.main/main.o.stamp : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o.stamp : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto_thin_archive_same_dir(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'bar.cpp', '-o', 'bar.o'])
      subprocess.check_call(
          [self.llvmar(), 'crsT', 'libfoobar.a', 'bar.o', 'foo.o'])
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'libfoobar.a',
          '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have rewrapper invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertIn('rewrapper ', buildrules)
        self.assertIn('build lto.main/main.o.stamp : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o.stamp : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_distributed_lto_thin_archive_subdir(self):
    self.run_archive_test(bitcode_archive=True,
                          bitcode_main=True,
                          thin_archive=True)

  def test_distributed_machine_code_thin_archive_bitcode_main_subdir(self):
    self.run_archive_test(bitcode_archive=False,
                          bitcode_main=True,
                          thin_archive=True)

  def test_distributed_machine_code_thin_archive_subdir(self):
    self.run_archive_test(bitcode_archive=False,
                          bitcode_main=False,
                          thin_archive=True)

  def test_distributed_bitcode_thick_archive_subdir(self):
    self.run_archive_test(bitcode_archive=True,
                          bitcode_main=True,
                          thin_archive=False)

  def test_distributed_machine_code_thick_archive_subdir(self):
    self.run_archive_test(bitcode_archive=False,
                          bitcode_main=False,
                          thin_archive=False)

  def test_distributed_machine_code_thick_archive_bitcode_main_subdir(self):
    self.run_archive_test(bitcode_archive=False,
                          bitcode_main=True,
                          thin_archive=False)

  def run_archive_test(self, bitcode_archive, bitcode_main, thin_archive):
    """
    Runs a test to ensure correct remote linking handling of
    an archive.
    Arguments:
    bitcode_archive: whether the archive should contain bitcode (true)
    or machine code (false)
    bitcode_main: whether the main object, outside the archive, should
    contain bitcode (true) or machine code (false)
    thin_archive: whether to create a thin archive instead of a regular archive.
    """
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'main.cpp', '-o', 'obj/main.o'] +
          _lto_args(bitcode_main))
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'foo.cpp', '-o', 'obj/foo.o'] +
          _lto_args(bitcode_archive))
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', 'bar.cpp', '-o', 'obj/bar.o'] +
          _lto_args(bitcode_archive))
      archive_creation_arg = 'crs'
      if thin_archive:
        archive_creation_arg = 'crsT'
      subprocess.check_call([
          self.llvmar(), archive_creation_arg, 'obj/libfoobar.a', 'obj/bar.o',
          'obj/foo.o'
      ])
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'obj/main.o',
          'obj/libfoobar.a', '-o', 'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      if bitcode_main or bitcode_archive:
        # build.ninja file should have rewrapper invocations in it.
        with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
          buildrules = f.read()
          self.assertIn('rewrapper ', buildrules)
          if bitcode_main:
            self.assertIn('build lto.main/obj/main.o.stamp : codegen ',
                          buildrules)
          if bitcode_archive:
            if thin_archive:
              self.assertIn('build lto.main/obj/foo.o.stamp : codegen ',
                            buildrules)
            else:
              self.assertIn(
                  'build lto.main/expanded_archives/obj/libfoobar.a/' +
                  'foo.o.stamp : codegen ', buildrules)
      # Check that main does not call foo.
      if bitcode_archive:
        disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
        main_idx = disasm.index(b' <main>:\n')
        after_main_idx = disasm.index(b'\n\n', main_idx)
        main_disasm = disasm[main_idx:after_main_idx]
        self.assertNotIn(b'foo', main_disasm)

  def test_debug_params(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangxx(), '-c', '-g', '-gsplit-dwarf', '-flto=thin', 'main.cpp',
          '-o', 'obj/main.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-g', '-gsplit-dwarf', '-flto=thin', 'foo.cpp',
          '-o', 'obj/foo.o'
      ])
      with open('main.rsp', 'w') as f:
        f.write('obj/main.o\n' 'obj/foo.o\n')
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', '-g', '-gsplit-dwarf',
          '-Wl,--lto-O2', '-o', 'main', '@main.rsp'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check debug info present, refers to .dwo file, and does not
      # contain full debug info for foo.cpp.
      dbginfo = subprocess.check_output(
          ['llvm-dwarfdump', '-debug-info',
           'main']).decode('utf-8', 'backslashreplace')
      self.assertRegexpMatches(dbginfo, '\\bDW_AT_GNU_dwo_name\\b.*\\.dwo"')
      self.assertNotRegexpMatches(dbginfo, '\\bDW_AT_name\\b.*foo\\.cpp"')

  def test_distributed_lto_params(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      os.makedirs('obj')
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'main.cpp', '-o', 'obj/main.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'foo.cpp', '-o', 'obj/foo.o'
      ])
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', '-m32', '-fsplit-lto-unit',
          '-fwhole-program-vtables', 'bar.cpp', '-o', 'obj/bar.o'
      ])
      subprocess.check_call(
          [self.llvmar(), 'crsT', 'obj/libfoobar.a', 'obj/bar.o', 'obj/foo.o'])
      with open('main.rsp', 'w') as f:
        f.write('-fsplit-lto-unit\n'
                '-fwhole-program-vtables\n'
                'obj/main.o\n'
                'obj/libfoobar.a\n')
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', '-m32', '-Wl,-mllvm',
          '-Wl,-generate-type-units', '-Wl,--lto-O2', '-o', 'main',
          '-Wl,--start-group', '@main.rsp', '-Wl,--end-group'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # Check codegen parameters.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        codegen_match = re.search('^rule codegen\\b.*?^[^ ]', buildrules,
                                  re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(codegen_match)
        codegen_text = codegen_match.group(0)
        self.assertIn('rewrapper', codegen_text)
        self.assertIn('-m32', codegen_text)
        self.assertIn('-mllvm -generate-type-units', codegen_text)
        self.assertNotIn('-flto', codegen_text)
        self.assertIn('build lto.main/obj/main.o.stamp : codegen ', buildrules)
        self.assertIn('build lto.main/obj/foo.o.stamp : codegen ', buildrules)
        link_match = re.search('^build main : native-link\\b.*?^[^ ]',
                               buildrules, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(link_match)
        link_text = link_match.group(0)
        self.assertIn('main.split.o', link_text)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_no_rewrapper(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = RemoteLinkUnixAllowMain().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--no-wrapper', '-j', '16', '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should not have rewrapper invocations in it.
      with open(os.path.join(d, 'lto.main', 'build.ninja')) as f:
        buildrules = f.read()
        self.assertNotIn('rewrapper ', buildrules)
        self.assertIn('build lto.main/main.o.stamp : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o.stamp : codegen ', buildrules)
      # Check that main does not call foo.
      disasm = subprocess.check_output(['llvm-objdump', '-d', 'main'])
      main_idx = disasm.index(b' <main>:\n')
      after_main_idx = disasm.index(b'\n\n', main_idx)
      main_disasm = disasm[main_idx:after_main_idx]
      self.assertNotIn(b'foo', main_disasm)

  def test_generate_no_codegen(self):
    with named_directory() as d, working_directory(d):
      with open('main.o', 'wb') as f:
        f.write(b'\7fELF')
      with mock.patch('sys.stderr', new_callable=StringIO) as stderr:
        rc = RemoteLinkUnixAllowMain().main([
            'remote_ld.py', '--ar-path',
            self.llvmar(), '--generate', '--',
            self.clangxx(), 'main.o', '-o', 'main'
        ])
        self.assertEqual(rc, 5)
        self.assertIn('no ninja file generated.\n', stderr.getvalue())

  def test_generate(self):
    with named_directory() as d, working_directory(d):
      with open('main.o', 'wb') as f:
        f.write(b'BC\xc0\xde')
      with mock.patch('sys.stderr', new_callable=StringIO) as stderr:
        rc = RemoteLinkUnixAllowMain().main([
            'remote_ld.py', '--ar-path',
            self.llvmar(), '--generate', '--',
            self.clangxx(), 'main.o', '-o', 'main'
        ])
        self.assertEqual(rc, 0)
        m = re.search('ninja file (.*)', stderr.getvalue())
        self.assertIsNotNone(m)
        path = shlex.split(m.group(1))[0]
        self.assertTrue(os.path.exists(path))
        content = open(path).read()
        self.assertRegex(
            content,
            re.compile('^build [^:]+/main\\.o\\.stamp : codegen ',
                       re.MULTILINE))

  def test_override_allowlist(self):
    with named_directory() as d, working_directory(d):
      _create_inputs(d)
      subprocess.check_call([
          self.clangxx(), '-c', '-Os', '-flto=thin', 'main.cpp', '-o', 'main.o'
      ])
      subprocess.check_call(
          [self.clangxx(), '-c', '-Os', '-flto=thin', 'foo.cpp', '-o', 'foo.o'])
      rc = remote_ld.RemoteLinkUnix().main([
          'remote_ld.py', '--ar-path',
          self.llvmar(), '--generate', '--allowlist', '--',
          self.clangxx(), '-fuse-ld=lld', '-flto=thin', 'main.o', 'foo.o', '-o',
          'main'
      ])
      # Should succeed.
      self.assertEqual(rc, 0)
      # build.ninja file should have rules for main and foo.
      ninjafile = os.path.join(d, 'lto.main', 'build.ninja')
      self.assertTrue(os.path.exists(ninjafile))
      with open(ninjafile) as f:
        buildrules = f.read()
        self.assertIn('build lto.main/main.o.stamp : codegen ', buildrules)
        self.assertIn('build lto.main/foo.o.stamp : codegen ', buildrules)


if __name__ == '__main__':
  unittest.main()
