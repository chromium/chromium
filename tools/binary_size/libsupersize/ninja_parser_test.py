#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from ninja_parser import (ParseOneFileForTest, _ParseNinjaPathList,
                          _GetOutputObject)


class NinjaParserTest(unittest.TestCase):

  def _ParseOneFile(self, line, lib, expected_inputs, expected_dep_map):
    """Exercises ninja_parser's ParseOneFile method.

    This allows coverage of most parsing capabilities, without having to
    synthesize the actual Ninja files expected by the top-level Parse()
    method.
    """
    dep_map = {}
    linker_inputs_map = {lib: []}
    ParseOneFileForTest([line], dep_map, linker_inputs_map)
    found_inputs = linker_inputs_map.get(lib)
    self.assertEqual(expected_inputs, found_inputs)
    self.assertEqual(expected_dep_map, dep_map)

  # These cases cover finding ELF outputs with associated inputs:

  def test_ExplicitDep(self):
    line = 'build libfoo.so: link a.o'
    inputs = ['a.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  def test_MultipleExplicitDeps(self):
    line = 'build libfoo.so: link a.o b.o'
    inputs = ['a.o', 'b.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  def test_ExplicitDepWithImplicitDep(self):
    line = 'build libfoo.so: link a.o | b.o'
    inputs = ['a.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  def test_ExplicitDepWithOrderDep(self):
    line = 'build libfoo.so: link a.o || b.o'
    inputs = ['a.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  def test_NoExplicitInput(self):
    line = 'build libfoo.so: custom_link | custom.py'
    inputs = []
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  def test_SpacesInPaths(self):
    line = r'build libfoo.so: link a\ a.o b\ b.o'
    inputs = ['a a.o', 'b b.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  # These cases cover Object file outputs that update a dependency map:

  def test_ObjectOutputWithExplicitDep(self):
    line = 'build libfoo.o: cxx a.cc'
    dep_map = {'libfoo.o': 'a.cc'}
    self._ParseOneFile(line, 'libfoo.o', [], dep_map)

  def test_ObjectOutputWithExplicitDeps(self):
    line = 'build libfoo.o: cxx a.cc b.cc'
    dep_map = {'libfoo.o': 'a.cc b.cc'}
    self._ParseOneFile(line, 'libfoo.o', [], dep_map)

  def test_ObjectOutputWithOrderDep(self):
    line = 'build libfoo.o: cxx a.cc || a.inputdeps.stamp'
    dep_map = {'libfoo.o': 'a.cc'}
    self._ParseOneFile(line, 'libfoo.o', [], dep_map)

  def test_ObjectOutputWithExplicitDepsAndOrderDep(self):
    line = 'build libfoo.o: cxx a.cc b.cc || a.inputdeps.stamp'
    dep_map = {'libfoo.o': 'a.cc b.cc'}
    self._ParseOneFile(line, 'libfoo.o', [], dep_map)

  def test_RlibOutputOnly(self):
    # This is the format before https://crrev.com/c/7316228
    line = 'build libfoo.rlib: rust a.rs'
    dep_map = {'libfoo.rlib': {'a.rs': 'a.rs'}}
    self._ParseOneFile(line, 'libfoo.rlib', [], dep_map)

  def test_RlibOutputAndOtherOutputs(self):
    # This is the format after https://crrev.com/c/7316228
    line = 'build libfoo.rlib foo.rustflags.json foo.rsp.rust: rust a.rs'
    dep_map = {'libfoo.rlib': {'a.rs': 'a.rs'}}
    self._ParseOneFile(line, 'libfoo.rlib', [], dep_map)


class NinjaParserHelpersTest(unittest.TestCase):

  def test_ParseNinjaPathList(self):
    outputs = r'aaa\ bbb.o ccc.o'
    split_outputs = list(_ParseNinjaPathList(outputs))
    self.assertEqual(['aaa bbb.o', 'ccc.o'], split_outputs)

  def test_GetOutputObject(self):
    self.assertEqual('a.o', _GetOutputObject('a.o'))
    self.assertEqual('b.o', _GetOutputObject('a.txt b.o c.txt'))
    self.assertEqual('a.rlib', _GetOutputObject('a.rlib a.rustflags.json'))

    # Not object file extension:
    self.assertEqual(None, _GetOutputObject('a.txt b.txt'))

    # More than one object file.  Example of a build rule where this happens
    # in practice can be found here:
    # https://source.chromium.org/chromium/chromium/src/+/main:out/linux-Debug/clang_x64_for_rust_host_build_tools/toolchain.ninja;l=125;drc=cf57ec2fd7f7fdf80d6be2de24871ce2bd164795
    self.assertEqual(None, _GetOutputObject('a.rlib b.rlib'))


if __name__ == '__main__':
  unittest.main()
