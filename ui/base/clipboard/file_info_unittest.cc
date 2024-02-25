// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/file_info.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(x) FILE_PATH_LITERAL(x)

namespace ui {

// Tests parsing from text/uri-list to list of FileInfo.
TEST(FileInfoTest, Roundtrip) {
  struct TestCase {
    std::string uri_list;
    std::vector<base::FilePath::StringType> paths;
    std::optional<std::string> uri_list_roundtrip;
  };
  const TestCase tests[] = {
      // Empty text/uri-list should give empty list.
      {"", {}},
      // Single path should give a single result.
      {"file:///path", {FPL("/path")}},
      // Multiple paths should be parsed.
      {"file:///path1\r\nfile:///path2", {FPL("/path1"), FPL("/path2")}},
      // Invalid URLs should be ignored.
      {"/path", {}},
      {"notfile:///path", {}},
      {"file:path", {}},
      {"file:/path", {}},
      {"file://", {}},
      // Network paths should be allowed.
      {"file://host", {FPL("//host")}},
      {"file://host/path", {FPL("//host/path")}},
      // Root filesystem '/'.
      {"file:///", {FPL("/")}},
      // Windows paths.
      {"file:///C:/path", {FPL("C:/path")}},
      {"file:///C:/", {FPL("C:/")}},
      // Not quite windows paths.
      {"file:///C:", {FPL("/C:")}},
      {"file:///CD:/path", {FPL("/CD:/path")}},
      {"file:///C:path", {FPL("/C:path")}},
      {"file:///:/path", {FPL("/:/path")}},
      // Encoded chars.
      {"file:///colon:", {FPL("/colon:")}},
      {"file:///colon%3A", {FPL("/colon:")}, "file:///colon:"},
      {"file:///truncated%", {FPL("/truncated%")}, "file:///truncated%25"},
      {"file:///truncated%1", {FPL("/truncated%1")}, "file:///truncated%251"},
      {"file:///percent%25", {FPL("/percent%")}},
      {"file:///percent%2525", {FPL("/percent%25")}},
      {"file:///space ", {FPL("/space")}, "file:///space"},
      {"file:///space%20", {FPL("/space ")}},
      {"file:///path%2525:%3B%23&=%0A%20", {FPL("/path%25:;#&=\n ")}},
  };
  for (const TestCase& test : tests) {
    std::vector<base::FilePath::StringType> expected;
    for (const auto& path : test.paths) {
      expected.push_back(
          base::FilePath(path).NormalizePathSeparators().value());
    }
    std::vector<FileInfo> file_infos = URIListToFileInfos(test.uri_list);
    std::vector<base::FilePath::StringType> actual;
    for (const FileInfo& file_info : file_infos) {
      actual.push_back(file_info.path.value());
    }
    EXPECT_EQ(expected, actual);
    if (!file_infos.empty()) {
      std::string uri_list = FileInfosToURIList(file_infos);
      EXPECT_EQ(test.uri_list_roundtrip.value_or(test.uri_list), uri_list);
    }
  }
}

TEST(FileInfoTest, Backslashes) {
  struct TestCase {
    base::FilePath::StringType path;
    std::string uri_list;
    std::optional<base::FilePath::StringType> path_roundtrip;
  };
  const TestCase tests[] = {
#if BUILDFLAG(IS_WIN)
    // File paths with backslash should roundtrip on windows.
    {FPL("C:\\path"), "file:///C:/path"},
    {FPL("\\path"), "file:///path"},
    {FPL("\\\\host\\path"), "file://host/path"},
#else
    // File paths with backslash should be escaped on posix, and relative path
    // becomes absolute path.
    {FPL("C:\\path"), "file:///C:%5Cpath", FPL("/C:\\path")},
    {FPL("\\path"), "file:///%5Cpath", FPL("/\\path")},
    {FPL("\\\\host\\path"), "file:///%5C%5Chost%5Cpath",
     FPL("/\\\\host\\path")},
#endif
  };
  for (const TestCase& test : tests) {
    FileInfo file_info(base::FilePath(test.path), base::FilePath());
    std::string uri_list = FileInfosToURIList({file_info});
    EXPECT_EQ(test.uri_list, uri_list);

    std::vector<FileInfo> filenames = URIListToFileInfos(uri_list);
    EXPECT_EQ(1u, filenames.size());
    EXPECT_EQ(test.path_roundtrip.value_or(test.path),
              filenames[0].path.value());
  }
}

}  // namespace ui
