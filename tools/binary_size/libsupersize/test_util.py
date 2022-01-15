# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import logging

import describe


class Golden:
  """Utility to use or manage "Golden" test files."""

  # Global state on whether to update Golden files in CheckOrUpdate().
  do_update = False

  @staticmethod
  def EnableUpdate():
    Golden.do_update = True

  @staticmethod
  def CheckOrUpdate(golden_path, actual_lines):
    if Golden.do_update:
      with open(golden_path, 'w') as file_obj:
        describe.WriteLines(actual_lines, file_obj.write)
      logging.info('Wrote %s', golden_path)
    else:
      with open(golden_path) as file_obj:
        expected = list(file_obj)
        actual = list(l + '\n' for l in actual_lines)
        assert actual == expected, (
            ('Did not match %s.\n' % golden_path) + ''.join(
                difflib.unified_diff(expected, actual, 'expected', 'actual')))
