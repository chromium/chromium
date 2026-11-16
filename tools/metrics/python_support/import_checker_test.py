#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import tempfile
import unittest

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.python_support.import_checker as import_checker


class FindSymbolImportsTest(unittest.TestCase):
  def setUp(self) -> None:
    temp_dir = tempfile.TemporaryDirectory()
    self.addCleanup(temp_dir.cleanup)
    # A source tree with the package chromium_src.pkg and its module mod.
    self.src_path = pathlib.Path(temp_dir.name)
    (self.src_path / 'pkg').mkdir()
    (self.src_path / 'pkg' / 'mod.py').touch()

  def _Find(
    self, file_text: str, changed_lines: set[int]
  ) -> list[tuple[int, str, str]]:
    return [
      (symbol_import.line, symbol_import.module, symbol_import.name)
      for symbol_import in import_checker.FindSymbolImports(
        file_text, changed_lines, self.src_path
      )
    ]

  def _FindOnAllLines(self, file_text: str) -> list[tuple[int, str, str]]:
    return self._Find(file_text, set(range(1, file_text.count('\n') + 1)))

  def testReportsClassesAndFunctionsFromChromiumSrc(self) -> None:
    self.assertEqual(
      self._FindOnAllLines('from chromium_src.pkg.mod import Thing, helper\n'),
      [
        (1, 'chromium_src.pkg.mod', 'Thing'),
        (1, 'chromium_src.pkg.mod', 'helper'),
      ],
    )

  def testAllowsModulesFromChromiumSrc(self) -> None:
    self.assertEqual(
      self._FindOnAllLines(
        'from chromium_src import pkg\nfrom chromium_src.pkg import mod\n'
      ),
      [],
    )

  def testReportsClassesAndFunctionsFromTheStandardLibrary(self) -> None:
    self.assertEqual(
      self._FindOnAllLines(
        'from pathlib import Path\n'
        'from collections import defaultdict\n'
        'from unittest.mock import patch as mock_patch\n'
        'from os.path import *\n'
      ),
      [
        (1, 'pathlib', 'Path'),
        (2, 'collections', 'defaultdict'),
        (3, 'unittest.mock', 'patch'),
        (4, 'os.path', '*'),
      ],
    )

  def testAllowsModulesFromTheStandardLibrary(self) -> None:
    self.assertEqual(
      self._FindOnAllLines(
        'from os import path\n'
        'from unittest import mock\n'
        'from xml.etree import ElementTree as ET\n'
      ),
      [],
    )

  def testAllowsExemptModules(self) -> None:
    self.assertEqual(
      self._FindOnAllLines(
        'from __future__ import annotations\n'
        'from collections.abc import Iterator\n'
        'from parameterized import parameterized\n'
        'from typing import Any\n'
      ),
      [],
    )

  def testIgnoresModulesThatCannotBeFound(self) -> None:
    self.assertEqual(
      self._FindOnAllLines(
        'from chromium_src.missing import Thing\n'
        'from package_that_is_not_installed import Thing\n'
      ),
      [],
    )

  def testIgnoresRelativeImports(self) -> None:
    self.assertEqual(
      self._FindOnAllLines('from . import mod\nfrom .mod import Thing\n'), []
    )

  def testOnlyReportsNamesOnChangedLines(self) -> None:
    file_text = (
      'from pathlib import (\n  Path,\n  PurePath,\n)\nfrom os import sep\n'
    )
    self.assertEqual(self._Find(file_text, {3}), [(3, 'pathlib', 'PurePath')])
    self.assertEqual(self._Find(file_text, set()), [])

  def testRaisesSyntaxErrorForInvalidPython(self) -> None:
    with self.assertRaises(SyntaxError):
      import_checker.FindSymbolImports('from import\n', {1}, self.src_path)


if __name__ == '__main__':
  unittest.main()
