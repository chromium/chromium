#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import xml.dom.minidom

import generate_allowlist_from_histograms_file

_EXPECTED_FILE_CONTENT = (
    """// Generated from generate_allowlist_from_histograms_file.py. \
Do not edit!

#ifndef TEST_TEST_H_
#define TEST_TEST_H_

#include <algorithm>
#include <string_view>

namespace test_namespace {

inline constexpr std::string_view kTestNameAllowList[] = {
  "All",
  "DownloadView",
  "PageInfoView",
};

constexpr bool IsValidTestName(std::string_view s) {
  return std::binary_search(
    std::cbegin(kTestNameAllowList),
    std::cend(kTestNameAllowList),
    s);
}

}  // namespace test_namespace

#endif  // TEST_TEST_H_
""")

_EXPECTED_VARIANT_LIST = ["All", "DownloadView", "PageInfoView"]

_TEST_VARIANT_INPUT = """
<variants name="BubbleName">
  <variant name="All"/>
  <variant name="DownloadView"/>
  <variant name="PageInfoView"/>
</variants>
"""

_EXPECTED_ENUM_LIST = [123, 456]

_TEST_ENUM_INPUT = """
<enum name="URLHashes">
  <int value="123" label="label1"/>
  <int value="456" label="label2"/>
</enum>
"""


class VariantAllowListTest(unittest.TestCase):

  def testGenerateSourceFileContent(self):
    namespace = "test_namespace"

    allow_list_name = "TestName"

    # Provide an unsorted list to ensure the list gets sorted since the check
    # function relies on being sorted.
    variant_list = ["DownloadView", "All", "PageInfoView"]
    content = generate_allowlist_from_histograms_file._GenerateStaticFile(
        "test/test.h", namespace, variant_list, allow_list_name)
    self.assertEqual(_EXPECTED_FILE_CONTENT, content)

  def testGenerateListFromVariants(self):
    histograms = xml.dom.minidom.parseString(_TEST_VARIANT_INPUT)
    allow_list_name = "BubbleName"
    variants = generate_allowlist_from_histograms_file._GenerateValueList(
        histograms, "variant", allow_list_name)
    self.assertEqual(_EXPECTED_VARIANT_LIST, variants)

    # Has incorrect allow list name.
    allow_list_name = "MissingVariantsName"
    with self.assertRaises(generate_allowlist_from_histograms_file.Error):
      generate_allowlist_from_histograms_file._GenerateValueList(
          histograms, "variant", allow_list_name)

  def testGenerateListFromEnums(self):
    histograms = xml.dom.minidom.parseString(_TEST_ENUM_INPUT)
    allow_list_name = "URLHashes"
    variants = generate_allowlist_from_histograms_file._GenerateValueList(
        histograms, "enum", allow_list_name)
    self.assertEqual(_EXPECTED_ENUM_LIST, variants)

    # Has incorrect allow list name.
    allow_list_name = "MissingVariantsName"
    with self.assertRaises(generate_allowlist_from_histograms_file.Error):
      generate_allowlist_from_histograms_file._GenerateValueList(
          histograms, "enum", allow_list_name)


if __name__ == "__main__":
  unittest.main()
