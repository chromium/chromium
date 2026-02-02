// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_linux.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui::clipboard_util {

TEST(ClipboardUtilLinuxTest, GetUriListFromPaths) {
  std::vector<std::string> paths = {"/path/to/file1.txt",
                                    "/path/to/file2 with spaces.txt"};
  std::string uri_list = GetUriListFromPaths(paths);
  EXPECT_EQ(uri_list,
            "file:///path/to/file1.txt\r\n"
            "file:///path/to/file2%20with%20spaces.txt");

  EXPECT_EQ(GetUriListFromPaths({}), "");
}

TEST(ClipboardUtilLinuxTest, GetPathsFromUriList) {
  std::string uri_list =
      "file:///path/to/file1.txt\r\n"
      "#comment\r\n"
      "file:///path/to/file2%20with%20spaces.txt\r\n"
      "http://example.com/not-a-file\r\n";

  std::vector<std::string> paths = GetPathsFromUriList(uri_list);

  ASSERT_EQ(paths.size(), 2u);
  EXPECT_EQ(paths[0], "/path/to/file1.txt");
  EXPECT_EQ(paths[1], "/path/to/file2 with spaces.txt");

  EXPECT_TRUE(GetPathsFromUriList("").empty());
}

}  // namespace ui::clipboard_util
