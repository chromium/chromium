# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.histograms.find_unmapped_histograms as find_unmapped_histograms


class FindHistogramsTest(unittest.TestCase):
  def test_reads_utf8_source_that_is_not_valid_cp1252(self) -> None:
    temp_dir = self.enterContext(tempfile.TemporaryDirectory())
    self.enterContext(contextlib.chdir(temp_dir))
    # U+00C1 is encoded as C3 81 in UTF-8, and 0x81 is undefined in cp1252,
    # the default locale encoding on Windows.
    pathlib.Path('foo.cc').write_text(
      '// \u00c1\nUMA_HISTOGRAM_BOOLEAN("Foo.Bar", true);\n', encoding='utf-8'
    )
    self.enterContext(
      mock.patch.object(
        find_unmapped_histograms,
        '_run_git',
        return_value='foo.cc:2:UMA_HISTOGRAM_BOOLEAN("Foo.Bar", true);',
      )
    )

    location_map = find_unmapped_histograms._find_histograms(
      'UMA_HISTOGRAM',
      find_unmapped_histograms.HISTOGRAM_MACRO_REGEX,
      find_unmapped_histograms.HISTOGRAM_MACRO_SUFFIXES,
      find_unmapped_histograms.OTHER_HISTOGRAM_MACROS,
      require_literals=True,
    )

    self.assertEqual(location_map, {'Foo.Bar': 'foo.cc:2'})


class MainTest(unittest.TestCase):
  def setUp(self) -> None:
    # main() changes into the scanned directory; restore the original one.
    self.enterContext(contextlib.chdir(os.getcwd()))
    self.git_check_dirs: list[str] = []
    self.enterContext(
      mock.patch.object(
        find_unmapped_histograms,
        '_git_cmd_available',
        side_effect=self._record_git_check,
      )
    )

  def _record_git_check(self, command: list[str]) -> bool:
    self.git_check_dirs.append(os.getcwd())
    return True

  def _exit_code_of_main(
    self, chromium_histograms: dict[str, str], xml_histograms: set[str]
  ) -> int | str | None:
    with (
      mock.patch.object(sys, 'argv', ['find_unmapped_histograms.py']),
      mock.patch.object(
        find_unmapped_histograms,
        '_read_chromium_histograms',
        return_value=chromium_histograms,
      ),
      mock.patch.object(
        find_unmapped_histograms,
        '_read_all_xml_histograms',
        return_value=xml_histograms,
      ),
      self.assertLogs(level='INFO'),
      self.assertRaises(SystemExit) as exit_context,
    ):
      find_unmapped_histograms.main()
    return exit_context.exception.code

  def test_scans_chromium_src_by_default(self) -> None:
    self._exit_code_of_main({}, set())

    self.assertEqual(self.git_check_dirs, [str(path_util.CHROMIUM_SRC_PATH)])

  def test_exit_code_is_one_when_256_histograms_are_unmapped(self) -> None:
    # An exit code equal to the number of unmapped histograms wraps around to 0
    # (success) on POSIX when that number is a multiple of 256.
    chromium_histograms = {
      f'Test.Unmapped{i}': f'foo.cc:{i}' for i in range(256)
    }

    exit_code = self._exit_code_of_main(chromium_histograms, set())

    self.assertEqual(exit_code, 1)

  def test_exit_code_is_zero_when_all_histograms_are_mapped(self) -> None:
    exit_code = self._exit_code_of_main(
      {'Test.Mapped': 'foo.cc:1'}, {'Test.Mapped'}
    )

    self.assertEqual(exit_code, 0)


if __name__ == '__main__':
  unittest.main()
