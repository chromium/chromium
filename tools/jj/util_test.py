#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import util

TWO_PARAGRAPHS = '''paragraph1

paragraph2'''

TRAILERS_TEXT = '''Change-Id: foo
Change-Id: bar
Bug: 123'''

TRAILERS_DICT = {
    'Change-Id': ['foo', 'bar'],
    'Bug': ['123'],
}


class TestUtil(unittest.TestCase):

  def test_split_description_interprets_single_line_trailer_as_desc(self):
    want_desc = 'WIP: blah'
    desc, trailers = util.split_description(want_desc)
    self.assertEqual(desc, want_desc)
    self.assertEqual(trailers, {})

  def test_split_description_no_trailers(self):
    desc, trailers = util.split_description(TWO_PARAGRAPHS)
    self.assertEqual(desc, TWO_PARAGRAPHS)
    self.assertEqual(trailers, {})

  def test_split_description_only_trailers(self):
    desc, trailers = util.split_description('\n\n' + TRAILERS_TEXT)
    self.assertEqual(desc, '')
    self.assertEqual(trailers, TRAILERS_DICT)

  def test_split_description_text_and_trailers(self):
    desc, trailers = util.split_description(
        f'{TWO_PARAGRAPHS}\n\n{TRAILERS_TEXT}')
    self.assertEqual(desc, TWO_PARAGRAPHS)
    self.assertEqual(trailers, TRAILERS_DICT)

  def test_split_description_mixed_trailers(self):
    desc, trailers = util.split_description(
        f'{TWO_PARAGRAPHS}\n\nparagraph3\n{TRAILERS_TEXT}\nnon_trailer')
    self.assertEqual(desc, TWO_PARAGRAPHS)
    self.assertEqual(trailers, TRAILERS_DICT)


if __name__ == '__main__':
  unittest.main()
