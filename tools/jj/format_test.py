#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import shutil
import subprocess
import sys
import unittest


class TestFormatter(unittest.TestCase):

  def run_formatter(self, path, code):
    vpython3 = shutil.which('vpython3')
    self.assertIsNotNone(vpython3)
    formatter = pathlib.Path(vpython3).parent / 'format.py'
    cmd = [shutil.which('python3'), formatter, path]
    ps = subprocess.run(
        cmd,
        input=code,
        text=True,
        stdout=subprocess.PIPE,
        check=False,
    )
    if not ps.returncode:
      return ps.stdout

  def test_python(self):
    self.assertEqual(
        self.run_formatter('foo.py', 'print( 1)'),
        'print(1)\n',
    )

    # *_pb2 is listed in .yapfignore
    self.assertEqual(
        self.run_formatter('foo_pb2.py', 'print( 1)'),
        'print( 1)',
    )

  def test_gn(self):
    self.assertEqual(
        self.run_formatter('foo.gn', 'print( 1)'),
        'print(1)\n',
    )

  def test_lucicfg(self):
    self.assertEqual(
        self.run_formatter('foo.star', 'print( 1)'),
        'print(1)\n',
    )

  def test_mojom(self):
    self.assertEqual(
        self.run_formatter('foo.mojom', 'module foo.mojom;\ninterface Foo{};'),
        'module foo.mojom;\n\ninterface Foo {};\n',
    )

  def test_go(self):
    self.assertEqual(
        self.run_formatter('foo.go', 'package main\nimport "fmt"'),
        'package main\n\nimport "fmt"\n',
    )

  def test_swift(self):
    if sys.platform != 'darwin':
      # swift-format only works on mac.
      self.assertEqual(
          self.run_formatter('foo.swift', 'print( 1)'),
          'print( 1)',
      )

  def test_java(self):
    self.assertEqual(
        self.run_formatter('foo.java', 'public class Foo{}'),
        'public class Foo {}\n',
    )

  def test_clang_format(self):
    self.assertEqual(
        self.run_formatter('foo.h',
                           '#include <utility>\n#include<algorithm>\n'),
        '#include <algorithm>\n#include <utility>\n',
    )


if __name__ == '__main__':
  unittest.main()
