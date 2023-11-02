#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit test for extracting resident pages."""

import random
import unittest

import extract_resident_pages

class ExtractResidentPagesUnittest(unittest.TestCase):

  def testParseResidentPages(self):
    max_pages = 10600
    pages = []
    resident_pages = ""

    for i in range(max_pages):
      is_resident = random.randint(0,1)
      pages.append(is_resident)
      if is_resident:
        resident_pages += str(i) + '\n'

    pages_list = extract_resident_pages.ParseResidentPages(resident_pages)

    for i in range(len(pages_list)):
      self.assertEqual(pages[i], pages_list[i])

    # As ParseResidentPages is only aware of the maximum page number that is
    # resident, check that all others are not resident.
    for i in range(len(pages_list), len(pages)):
      self.assertFalse(pages[i])


if __name__ == '__main__':

  unittest.main()
