#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import unittest
import xml.dom.minidom

import generate_expired_histograms_array

_EXPECTED_HEADER_FILE_CONTENT = (
"""// Generated from generate_expired_histograms_array.py. Do not edit!

#ifndef TEST_TEST_H_
#define TEST_TEST_H_

#include <stdint.h>

namespace some_namespace {{

// Contains hashes of expired histograms.
{array_definition}

}}  // namespace some_namespace

#endif  // TEST_TEST_H_
""")

_EXPECTED_NON_EMPTY_ARRAY_DEFINITION = (
"""const uint64_t kExpiredHistogramsHashes[] = {
  0x965ce8e9e12a9c89,  // Test.FirstHistogram
  0xdb5b2f55ffd139e8,  // Test.SecondHistogram
};

const size_t kNumExpiredHistograms = 2;"""
)

_EXPECTED_EMPTY_ARRAY_DEFINITION = (
"""const uint64_t kExpiredHistogramsHashes[] = {
  0x0000000000000000,  // Dummy.Histogram
};

const size_t kNumExpiredHistograms = 1;"""
)

class ExpiredHistogramsTest(unittest.TestCase):

  def testGetExpiredHistograms(self):
    histograms = {
        "FirstHistogram": {
            "expires_after": "2000-10-01"
        },
        "SecondHistogram": {
            "expires_after": "2002-10-01"
        },
        "ThirdHistogram": {
            "expires_after": "2001-10-01"
        },
        "FourthHistogram": {},
        "FifthHistogram": {
            "obsolete": "Has expired.",
            "expires_after": "2000-10-01"
        },
        "SixthHistogram": {
            "expires_after": "M22"
        },
        "SeventhHistogram": {
            "expires_after": "M60"
        },
        "EigthHistogram": {
            "expires_after": "M65"
        },
    }

    base_date = datetime.date(2001, 10, 1)
    current_milestone = 60

    expired_histograms_names = (
        generate_expired_histograms_array._GetExpiredHistograms(
            histograms, base_date, current_milestone))

    self.assertEqual(2, len(expired_histograms_names))
    self.assertIn("FirstHistogram", expired_histograms_names)
    self.assertIn("SixthHistogram", expired_histograms_names)

  def testBadExpiryDate(self):
    histograms = {
        "FirstHistogram": {
            "expires_after": "2000-10-01"
        },
        "SecondHistogram": {
            "expires_after": "2000/10/01"
        }
    }
    base_date = datetime.date(2000, 10, 1)
    current_milestone = 60

    with self.assertRaises(generate_expired_histograms_array.Error) as error:
        generate_expired_histograms_array._GetExpiredHistograms(histograms,
            base_date, current_milestone)

    self.assertEqual(
        generate_expired_histograms_array._DATE_FORMAT_ERROR.format(
            date="2000/10/01", name="SecondHistogram"), str(error.exception))

  def testGetBaseDate(self):
    regex = generate_expired_histograms_array._DATE_FILE_RE

    # Does not match the pattern.
    content = "MAJOR_BRANCH__FAKE_DATE=2017-09-09"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, regex)

    # Has invalid format.
    content = "MAJOR_BRANCH_DATE=2010/01/01"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, regex)

    # Has invalid format.
    content = "MAJOR_BRANCH_DATE=2010-20-02"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, regex)

    # Has invalid date.
    content = "MAJOR_BRANCH_DATE=2017-02-29"
    with self.assertRaises(generate_expired_histograms_array.Error):
        generate_expired_histograms_array._GetBaseDate(content, regex)

    content = "!!FOO!\nMAJOR_BRANCH_DATE=2010-01-01\n!FOO!!"
    base_date = generate_expired_histograms_array._GetBaseDate(content, regex)
    self.assertEqual(base_date, datetime.date(2010, 1, 1))

  def testGenerateHeaderFileContent(self):
    header_filename = "test/test.h"
    namespace = "some_namespace"

    histogram_map = generate_expired_histograms_array._GetHashToNameMap(
        ["Test.FirstHistogram", "Test.SecondHistogram"])
    expected_histogram_map = {
        "0x965ce8e9e12a9c89": "Test.FirstHistogram",
        "0xdb5b2f55ffd139e8": "Test.SecondHistogram",
    }
    self.assertEqual(expected_histogram_map, histogram_map)

    content = generate_expired_histograms_array._GenerateHeaderFileContent(
        header_filename, namespace, histogram_map)

    self.assertEqual(_EXPECTED_HEADER_FILE_CONTENT.format(
        array_definition=_EXPECTED_NON_EMPTY_ARRAY_DEFINITION), content)

  def testGenerateFileHistogramExpiryWithGrace(self):
    histograms = xml.dom.minidom.parseString("""
<histogram-configuration>
<histograms><!-- Must be alphabetical. -->
  <histogram name="FirstHistogram" expires_after="2010-11-01" units="units">
    <owner>me@chromium.org</owner>
    <summary>
      This is a summary.
    </summary>
  </histogram>
  <histogram name="FourthHistogram" expires_after="M61" units="units">
    <owner>me@chromium.org</owner>
    <summary>
      This is a summary.
    </summary>
  </histogram>
  <histogram name="SecondHistogram" expires_after="2010-09-01" units="units">
    <owner>me@chromium.org</owner>
  <summary>
    This is a summary.
  </summary>
    </histogram>
  <histogram name="ThirdHistogram" expires_after="M60" units="units">
    <owner>me@chromium.org</owner>
    <summary>
      This is a summary.
    </summary>
  </histogram>
</histograms>
</histogram-configuration>
""")

    branch_data = "MAJOR_BRANCH_DATE=2011-01-01\n"
    mstone_data = "MAJOR=63\n"

    content = generate_expired_histograms_array._GenerateFileContent(
        histograms, branch_data, mstone_data, "header.h", "uma")
    # These have expired but are within the 14-week/2-milestone grace period.
    self.assertNotIn("FirstHistogram", content);
    self.assertNotIn("FourthHistogram", content);
    # These have expired and are outside of the grace period.
    self.assertIn("SecondHistogram", content);
    self.assertIn("ThirdHistogram", content);

  def testGenerateHeaderFileContentEmptyArray(self):
    header_filename = "test/test.h"
    namespace = "some_namespace"
    content = generate_expired_histograms_array._GenerateHeaderFileContent(
        header_filename, namespace, dict())
    self.assertEqual(_EXPECTED_HEADER_FILE_CONTENT.format(
        array_definition=_EXPECTED_EMPTY_ARRAY_DEFINITION), content)


if __name__ == "__main__":
  unittest.main()
