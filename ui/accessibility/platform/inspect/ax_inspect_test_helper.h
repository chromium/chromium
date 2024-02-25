// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_TEST_HELPER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_TEST_HELPER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {

class AXInspectScenario;

// A helper class for writing accessibility tree dump tests.
class AXInspectTestHelper {
 public:
  explicit AXInspectTestHelper(AXApiType::Type type);
  explicit AXInspectTestHelper(const char* expectation_type);
  ~AXInspectTestHelper() = default;

  // Overrides the expectation type. Useful to tune up the expectations format
  // after the helper object was instantiated.
  void OverrideExpectationType(const std::string& expectation_type) {
    expectation_type_ = expectation_type;
  }

  // Returns a path to an expectation file for the current platform. If no
  // suitable expectation file can be found, logs an error message and returns
  // an empty path.
  base::FilePath GetExpectationFilePath(
      const base::FilePath& test_file_path,
      const base::FilePath::StringType& expectations_qualifier =
          FILE_PATH_LITERAL(""));

  // Enable/disable features as needed.
  void InitializeFeatureList();
  void ResetFeatureList();

  // Parses a given testing scenario. Prepends default property filters if any
  // so the test file filters will take precedence over default filters in case
  // of conflict.
  AXInspectScenario ParseScenario(
      const std::vector<std::string>& lines,
      const std::vector<AXPropertyFilter>& default_filters = {});

  // Parses a given testing scenario from a file. Prepends default property
  // filters if any so the test file filters will take precedence over default
  // filters in case of conflict.
  std::optional<AXInspectScenario> ParseScenario(
      const base::FilePath& scenario_path,
      const std::vector<AXPropertyFilter>& default_filters = {});

  // Returns a platform-dependent list of inspect types used in dump tree
  // testing.
  static std::vector<AXApiType::Type> TreeTestPasses();

  // Returns a platform-dependent list of inspect types used in dump events
  // testing.
  static std::vector<AXApiType::Type> EventTestPasses();

  // Loads the given expectation file and returns the contents. An expectation
  // file may be empty, in which case an empty vector is returned.
  // Returns nullopt if the file contains a skip marker.
  static std::optional<std::vector<std::string>> LoadExpectationFile(
      const base::FilePath& expected_file);

  // Compares the given actual dump against the given expectation and generates
  // a new expectation file if switches::kGenerateAccessibilityTestExpectations
  // has been set. Returns true if the result matches the expectation.
  static bool ValidateAgainstExpectation(
      const base::FilePath& test_file_path,
      const base::FilePath& expected_file,
      const std::vector<std::string>& actual_lines,
      const std::vector<std::string>& expected_lines);

 private:
  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  //
  // In order to support multiple tests for the same html file, an
  // optional expectations_qualifier string may be specified. For
  // example, we could have both dump-tree and dump-node tests:
  // HTML test:      test-file.html
  // Expected:       test-file-node-expected-mac.txt
  // Expected:       test-file-tree-expected-mac.txt
  base::FilePath::StringType GetExpectedFileSuffix(
      const base::FilePath::StringType& expectations_qualifier =
          FILE_PATH_LITERAL("")) const;

  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  base::FilePath::StringType GetVersionSpecificExpectedFileSuffix(
      const base::FilePath::StringType& expectations_qualifier =
          FILE_PATH_LITERAL("")) const;

  FRIEND_TEST_ALL_PREFIXES(AXInspectTestHelperTest, TestDiffLines);

  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  static std::vector<int> DiffLines(
      const std::vector<std::string>& expected_lines,
      const std::vector<std::string>& actual_lines);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::string expectation_type_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_TEST_HELPER_H_
