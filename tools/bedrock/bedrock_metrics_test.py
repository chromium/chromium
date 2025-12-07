#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import os
import tempfile
import shutil

import bedrock_metrics


class TestBedrockMetrics(unittest.TestCase):

  def setUp(self):
    # Create a temporary directory for test files.
    self.test_dir = tempfile.mkdtemp(prefix="bedrock_test_")
    self.sub_dir = os.path.join(self.test_dir, "sub")
    os.makedirs(self.sub_dir)

    # --- Files for count_matching_files ---
    # Structure:
    # self.test_dir/
    #   - file_alpha.h (Content: "apple banana\ncount_me_once")
    #   - file_beta.cc (Content: "banana cherry\ncount_me_once\ncount_me_once")
    #   - file_gamma.mm (Content: "apple cherry\nno_count_string")
    #   - sub/
    #     - file_delta.h (Content: "apple\ncount_me_once")
    #   - file_exclude_me.cc (Content: "apple banana\ncontent for exclude")
    #   - other_extension.txt (Content: "apple banana\ntext file content")
    #   - empty_file.h (Content: "")
    files_to_create_for_matching = [
        ("file_alpha.h", "apple banana\ncount_me_once"),
        ("file_beta.cc", "banana cherry\ncount_me_once\ncount_me_once"),
        ("file_gamma.mm", "apple cherry\nno_count_string"),
        (os.path.join("sub", "file_delta.h"), "apple\ncount_me_once"),
        ("file_exclude_me.cc", "apple banana\ncontent for exclude"),
        ("other_extension.txt",
         "apple banana\ntext file content"),  # Invalid extension
        ("empty_file.h", "")
    ]

    for rel_path, content in files_to_create_for_matching:
      full_path = os.path.join(self.test_dir, rel_path)
      os.makedirs(os.path.dirname(full_path), exist_ok=True)
      with open(full_path, 'w', encoding='utf-8') as f:
        f.write(content)

    # --- Files for count_lines ---
    self.lines_file1_path = os.path.join(self.test_dir, "lines_file1.data")
    with open(self.lines_file1_path, 'w', encoding='utf-8') as f:
      f.write("line1\nline2\nline3")  # 3 lines

    self.lines_file2_path = os.path.join(self.test_dir, "lines_file2.data")
    with open(self.lines_file2_path, 'w', encoding='utf-8') as f:
      f.write("single line")  # 1 line

    self.lines_empty_file_path = os.path.join(self.test_dir, "lines_empty.data")
    with open(self.lines_empty_file_path, 'w', encoding='utf-8') as f:
      f.write("")  # 0 lines

  def tearDown(self):
    # Remove the temporary directory and its contents after tests.
    shutil.rmtree(self.test_dir)

  # --- Tests for count_matching_files ---

  def test_cmf_no_filters_counts_valid_extensions(self):
    # Expected: file_alpha.h, file_beta.cc, file_gamma.mm, sub/file_delta.h,
    # file_exclude_me.cc, empty_file.h (6 files with .h, .cc, .mm)
    # other_extension.txt is ignored due to extension.
    # No content_match_strings, so string_matches_count is 0.
    count, str_count = bedrock_metrics.count_matching_files(self.test_dir)
    self.assertEqual(count, 6, "File count mismatch with no filters")
    self.assertEqual(str_count, 0,
                     "String count should be 0 with no content search strings")

  def test_cmf_include_filename_single(self):
    # include_filename_strings=["alpha"] -> matches file_alpha.h
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_filename_strings=["alpha"])
    self.assertEqual(count, 1)
    self.assertEqual(str_count, 0)

  def test_cmf_include_filename_multiple_all_match(self):
    # include_filename_strings=["file", "alpha"] -> matches file_alpha.h
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_filename_strings=["file", "alpha"])
    self.assertEqual(count, 1)
    self.assertEqual(str_count, 0)

  def test_cmf_include_filename_multiple_one_no_match(self):
    # include_filename_strings=["alpha", "beta"] -> No file has BOTH "alpha"
    # AND"beta".
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_filename_strings=["alpha", "nomatch"])
    self.assertEqual(count, 0)
    self.assertEqual(str_count, 0)

  def test_cmf_exclude_filename_single(self):
    # exclude_filename_strings=["exclude"] -> Excludes file_exclude_me.cc
    # Original 6 valid files - 1 = 5 files.
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, exclude_filename_strings=["exclude"])
    self.assertEqual(count, 5)
    self.assertEqual(str_count, 0)

  # These tests verify the current code behavior (ANY).
  def test_cmf_include_file_content_strings_any_behavior_one_present(self):
    # include_file_content_strings=["apple"]
    # Matches: file_alpha.h, file_gamma.mm, sub/file_delta.h, file_exclude_me.cc
    # (4 files)
    # file_beta.cc (banana cherry) does not contain "apple". empty_file.h is
    # empty.
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_file_content_strings=["apple"])
    self.assertEqual(count, 4)
    self.assertEqual(str_count, 0)

  def test_cmf_include_file_content_strings_any_behavior_multiple_options(self):
    # include_file_content_strings=["apple", "cherry"] (any of these in content)
    # file_alpha.h ("apple") - Yes
    # file_beta.cc ("cherry") - Yes
    # file_gamma.mm ("apple", "cherry") - Yes
    # sub/file_delta.h ("apple") - Yes
    # file_exclude_me.cc ("apple") - Yes
    # empty_file.h - No
    # Expected: 5 files
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_file_content_strings=["apple", "cherry"])
    self.assertEqual(count, 5)
    self.assertEqual(str_count, 0)

  def test_cmf_include_file_content_strings_none_present_in_content(self):
    # include_file_content_strings=["nonexistent_string_pattern"]
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir,
        include_file_content_strings=["nonexistent_string_pattern"])
    self.assertEqual(count, 0)
    self.assertEqual(str_count, 0)

  def test_cmf_content_match_strings_single_term_count(self):
    # content_match_strings=["count_me_once"]
    # All 6 valid files are checked for content counts.
    # file_alpha.h: 1 | file_beta.cc: 2 | file_gamma.mm: 0
    # sub/file_delta.h: 1 | file_exclude_me.cc: 0 | empty_file.h: 0
    # Total string matches: 1 + 2 + 0 + 1 + 0 + 0 = 4
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, content_match_strings=["count_me_once"])
    self.assertEqual(
        count, 6,
        "File count should include all valid files for content counting")
    self.assertEqual(str_count, 4, "String match count error")

  def test_cmf_content_match_strings_multiple_terms_count(self):
    # content_match_strings=["apple", "banana"]
    # All 6 valid files are checked.
    # file_alpha.h ("apple banana"): apple(1)+banana(1)=2
    # file_beta.cc ("banana cherry"): apple(0)+banana(1)=1
    # file_gamma.mm ("apple cherry"): apple(1)+banana(0)=1
    # sub/file_delta.h ("apple"): apple(1)+banana(0)=1
    # file_exclude_me.cc ("apple banana"): apple(1)+banana(1)=2
    # empty_file.h: apple(0)+banana(0)=0
    # Total string matches: 2+1+1+1+2+0 = 7
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, content_match_strings=["apple", "banana"])
    self.assertEqual(count, 6)
    self.assertEqual(str_count, 7)

  def test_cmf_all_filters_combined(self):
    # include_filename_strings=["file"], exclude_filename_strings=["exclude"]
    # include_file_content_strings=["apple"] (content filter, using 'any' logic
    # of code)
    # content_match_strings=["count_me_once"]

    # 1. Include "file": file_alpha.h, file_beta.cc, file_gamma.mm,
    #    file_exclude_me.cc, empty_file.h
    # 2. Exclude "exclude": Removes file_exclude_me.cc.
    #    Remaining: file_alpha.h, file_beta.cc, file_gamma.mm, empty_file.h
    # 3. include_file_content_strings=["apple"]: (file must contain "apple")
    #    file_alpha.h (has "apple") - Keep
    #    file_beta.cc (no "apple") - Remove
    #    file_gamma.mm (has "apple") - Keep
    #    sub/file_delta.h (has "apple"): - Keep
    #    empty_file.h (no "apple") - Remove
    #    Matched files: file_alpha.h, file_gamma.mm. So, count = 2.
    # 4. content_match_strings=["count_me_once"] on these 2 files:
    #    file_alpha.h: 1 occurrence
    #    file_gamma.mm: 0 occurrences
    #    sub/file_delta.h (has "apple"): 1 occurrences
    #    Total str_count = 1.
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir,
        include_file_content_strings=["apple"],
        content_match_strings=["count_me_once"],
        include_filename_strings=["file"],
        exclude_filename_strings=["exclude"])
    self.assertEqual(count, 3)
    self.assertEqual(str_count, 2)

  def test_cmf_empty_directory(self):
    empty_subdir_path = os.path.join(self.test_dir, "empty_subdir_for_test")
    os.makedirs(empty_subdir_path)
    count, str_count = bedrock_metrics.count_matching_files(empty_subdir_path)
    self.assertEqual(count, 0)
    self.assertEqual(str_count, 0)

  def test_cmf_no_matching_extension_files_present(self):
    # Directory with only a .txt file, which is not in [.h, .cc, .mm]
    other_ext_dir = os.path.join(self.test_dir, "other_ext_dir")
    os.makedirs(other_ext_dir)
    with open(os.path.join(other_ext_dir, "somefile.txt"), 'w') as f:
      f.write("content")
    count, str_count = bedrock_metrics.count_matching_files(other_ext_dir)
    self.assertEqual(count, 0)
    self.assertEqual(str_count, 0)

  def test_cmf_include_file_content_strings_is_empty_list(self):
    # An empty list for include_file_content_strings means the content filter
    # (Step 4) is skipped.
    # Same as include_file_content_strings=None.
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, include_file_content_strings=[])
    self.assertEqual(count, 6)  # All 6 valid extension files
    self.assertEqual(str_count, 0)

  def test_cmf_content_match_strings_is_empty_list(self):
    # No strings to count, so str_count should be 0.
    # File count reflects all files matching other criteria.
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, content_match_strings=[])
    self.assertEqual(count, 6)  # All 6 valid extension files
    self.assertEqual(str_count, 0)

  def test_cmf_content_match_strings_includes_empty_string(self):
    # The code `if s_str:` skips empty strings in content_match_strings.
    # So, ["count_me_once", ""] should behave like ["count_me_once"].
    count, str_count = bedrock_metrics.count_matching_files(
        self.test_dir, content_match_strings=["count_me_once", ""])
    self.assertEqual(count, 6)
    self.assertEqual(str_count, 4)  # Same as just ["count_me_once"]

  # --- Tests for count_lines ---

  def test_cl_single_file_multiple_lines(self):
    self.assertEqual(bedrock_metrics.count_lines([self.lines_file1_path]), 3)

  def test_cl_single_file_one_line_no_trailing_newline(self):
    self.assertEqual(bedrock_metrics.count_lines([self.lines_file2_path]), 1)

  def test_cl_multiple_files(self):
    total_lines = bedrock_metrics.count_lines(
        [self.lines_file1_path, self.lines_file2_path])
    self.assertEqual(total_lines, 3 + 1)  # 4

  def test_cl_empty_file(self):
    # An empty file has 0 lines according to file.readlines().
    self.assertEqual(bedrock_metrics.count_lines([self.lines_empty_file_path]),
                     0)

  def test_cl_mixed_files_including_empty(self):
    total_lines = bedrock_metrics.count_lines([
        self.lines_file1_path, self.lines_empty_file_path, self.lines_file2_path
    ])
    self.assertEqual(total_lines, 3 + 0 + 1)  # 4

  def test_cl_empty_file_list(self):
    self.assertEqual(bedrock_metrics.count_lines([]), 0)


if __name__ == '__main__':
  unittest.main(verbosity=2)
