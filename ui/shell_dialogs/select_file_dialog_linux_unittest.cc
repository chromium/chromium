// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_linux.h"

#include <fnmatch.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

bool Matches(const std::string& pattern, const char* file_name) {
  return fnmatch(pattern.c_str(), file_name, 0) == 0;
}

}  // namespace

TEST(SelectFileDialogLinuxTest, CaseInsensitiveExtensionPattern) {
  EXPECT_EQ(SelectFileDialogLinux::GetCaseInsensitiveExtensionPattern("Rmd"),
            "*.[rR][mM][dD]");
  // Characters without ASCII case are kept as they are.
  EXPECT_EQ(SelectFileDialogLinux::GetCaseInsensitiveExtensionPattern("tar.gz"),
            "*.[tT][aA][rR].[gG][zZ]");
  EXPECT_EQ(SelectFileDialogLinux::GetCaseInsensitiveExtensionPattern("mp3"),
            "*.[mM][pP]3");
}

// Mixed-case file names must match regardless of the extension's case
// (https://crbug.com/41486306).
TEST(SelectFileDialogLinuxTest, CaseInsensitiveExtensionPatternMatches) {
  const std::string pattern =
      SelectFileDialogLinux::GetCaseInsensitiveExtensionPattern("rmd");
  EXPECT_TRUE(Matches(pattern, "invoice.rmd"));
  EXPECT_TRUE(Matches(pattern, "invoice.RMD"));
  EXPECT_TRUE(Matches(pattern, "invoice.Rmd"));
  EXPECT_TRUE(Matches(pattern, "invoice.rMd"));
  EXPECT_FALSE(Matches(pattern, "invoice.md"));
  EXPECT_FALSE(Matches(pattern, "invoice.rmdx"));
  EXPECT_FALSE(Matches(pattern, "invoice.txt"));
}

}  // namespace ui
