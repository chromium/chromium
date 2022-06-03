#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import ninja_parser


class NinjaParserTest(unittest.TestCase):

  def _ParseOneFile(self, line, lib, expected_inputs, expected_dep_map):
    """Exercises ninja_parser's ParseOneFile method.

    This allows coverage of most parsing capabilities, without having to
    synthesize the actual Ninja files expected by the top-level Parse()
    method.
    """
    dep_map = {}
    _, found_inputs = ninja_parser.ParseOneFileForTest([line], dep_map, lib)
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
    line = 'build libfoo.so: link a\ a.o b\ b.o'
    inputs = ['a a.o', 'b b.o']
    self._ParseOneFile(line, 'libfoo.so', inputs, {})

  # These cases cover Object file outputs that update a dependency map:

  def test_ObjectOutputWithExplicitDep(self):
    line = 'build libfoo.o: cxx a.cc'
    dep_map = {'libfoo.o': 'a.cc'}
    self._ParseOneFile(line, 'libfoo.o', None, dep_map)

  def test_ObjectOutputWithExplicitDeps(self):
    line = 'build libfoo.o: cxx a.cc b.cc'
    dep_map = {'libfoo.o': 'a.cc b.cc'}
    self._ParseOneFile(line, 'libfoo.o', None, dep_map)

  def test_ObjectOutputWithOrderDep(self):
    line = 'build libfoo.o: cxx a.cc || a.inputdeps.stamp'
    dep_map = {'libfoo.o': 'a.cc'}
    self._ParseOneFile(line, 'libfoo.o', None, dep_map)

  def test_ObjectOutputWithExplicitDepsAndOrderDep(self):
    line = 'build libfoo.o: cxx a.cc b.cc || a.inputdeps.stamp'
    dep_map = {'libfoo.o': 'a.cc b.cc'}
    self._ParseOneFile(line, 'libfoo.o', None, dep_map)


if __name__ == '__main__':
  unittest.main()
