# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from parameterized import parameterized
from xml.dom import minidom
import unittest

import split_xml

class SplitXmlTest(unittest.TestCase):

  @parameterized.expand([
      ('Camel case', 'MyHistogram.ThisHistogram', 'My'),
      ('All upper case', 'UMA', 'UMA'),
      ('In the predefined map', 'SafeBrowsing.TestHist', 'SafeBrowsing'),
  ])
  def testGetCamelCaseName(self, _, name, expected_name):
    doc = minidom.Document()
    node = doc.createElement('histogram')
    node.setAttribute('name', name)
    result = split_xml._GetCamelCaseName(node)
    self.assertEqual(expected_name, result)

  @parameterized.expand([
      ('Camel case', 'MyHistogram', 'my_histogram'),
      ('All upper case', 'UMA', 'uma'),
      ('mixed case', 'MYHistogram', 'my_histogram'),
      ('usual case followed by all upper case', 'MyHISTOGRAM', 'my_histogram')
  ])
  def testCamelCaseToSnakeCase(self, _, name, expected_name):
    result = split_xml._CamelCaseToSnakeCase(name)
    self.assertEqual(expected_name, result)


if __name__ == "__main__":
  unittest.main()