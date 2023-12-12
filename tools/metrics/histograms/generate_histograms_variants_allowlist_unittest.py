#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import xml.dom.minidom

import generate_histograms_variants_allowlist

_EXPECTED_FILE_CONTENT = (
    """// Generated from generate_histograms_variants_allowlist.py. Do not edit!

#ifndef TEST_TEST_H_
#define TEST_TEST_H_

#include <algorithm>
#include <string_view>

namespace test_namespace {

inline constexpr std::string_view kTestNameVariantAllowList[] = {
  "All",
  "DownloadView",
  "PageInfoView",
};

constexpr bool IsValidTestNameVariant(std::string_view s) {
  return std::binary_search(
    std::cbegin(kTestNameVariantAllowList),
    std::cend(kTestNameVariantAllowList),
    s);
}

}  // namespace test_namespace

#endif  // TEST_TEST_H_
""")

_EXPECTED_VARIANT_LIST = [{
    'name': 'All',
    'summary': 'All'
}, {
    'name': 'DownloadView',
    'summary': 'DownloadView'
}, {
    'name': 'PageInfoView',
    'summary': 'PageInfoView'
}]


class VariantAllowListTest(unittest.TestCase):
  def testGenerateSourceFileContent(self):
    namespace = "test_namespace"

    allow_list_name = "TestName"

    # Provide an unsorted list to ensure the list gets sorted since the check
    # function relies on being sorted.
    variant_list = [{
        'name': 'DownloadView'
    }, {
        'name': 'All'
    }, {
        'name': 'PageInfoView'
    }]
    content = generate_histograms_variants_allowlist._GenerateStaticFile(
        "test/test.h", namespace, variant_list, allow_list_name)
    self.assertEqual(_EXPECTED_FILE_CONTENT, content)

  def testGenerateVariantList(self):
    histograms = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms>
<variants name="BubbleName">
  <variant name="All"/>
  <variant name="DownloadView"/>
  <variant name="PageInfoView"/>
</variants>

<histogram name="Bubble.{BubbleName}.CloseReason" enum="WidgetClosedReason"
    expires_after="2024-09-01">
  <summary>Records the reason a bubble was closed.</summary>
  <token key="BubbleName" variants="BubbleName"/>
</histogram>
</histograms>
</histogram-configuration>
""")
    allow_list_name = "BubbleName"
    variants = generate_histograms_variants_allowlist._GenerateVariantList(
        histograms, allow_list_name)
    self.assertEqual(_EXPECTED_VARIANT_LIST, variants)

    # Has incorrect allow list name.
    allow_list_name = "MissingVariantsName"
    with self.assertRaises(generate_histograms_variants_allowlist.Error):
      generate_histograms_variants_allowlist._GenerateVariantList(
          histograms, allow_list_name)


if __name__ == "__main__":
  unittest.main()
