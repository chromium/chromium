# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for grd_helper.py."""

import io
import os
import sys
import unittest

# Add the parent dir so that we can import from "helper".
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from helper import grd_helper

here = os.path.dirname(os.path.realpath(__file__))
repo_root = os.path.normpath(os.path.join(here, '..', '..', '..'))


def read_file_as_text(path):
  with io.open(path, mode='r', encoding='utf-8') as f:
    return f.read()


class GrdHelperTest(unittest.TestCase):

  def testReadGrdMessages(self):
    grd_dir = os.path.join(repo_root, 'tools', 'translation', 'testdata')
    messages = grd_helper.GetGrdMessages(
        os.path.join(grd_dir, 'test.grd'), grd_dir)
    # Result should contain only the messages in test.grd. Even though the file
    # includes part.grdp via a <part> tag, part.grdp's messages should not be
    # listed here.
    self.assertTrue('IDS_TEST_STRING1' in messages)
    self.assertTrue('IDS_TEST_STRING2' in messages)
    self.assertFalse('IDS_TEST_STRING_NON_TRANSLATEABLE' in messages)

  def testReadGrdpMessages(self):
    messages = grd_helper.GetGrdpMessagesFromString(
        read_file_as_text(
            os.path.join(repo_root, 'tools', 'translation', 'testdata',
                         'part.grdp')))
    self.assertTrue('IDS_PART_STRING1' in messages)
    self.assertTrue('IDS_PART_STRING2' in messages)
    print('DONE')


if __name__ == '__main__':
  unittest.main()
