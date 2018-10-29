// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/execute_select_file_win.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

TEST(ShellDialogsWin, AppendExtensionIfNeeded) {
  struct AppendExtensionTestCase {
    const wchar_t* filename;
    const wchar_t* filter_selected;
    const wchar_t* suggested_ext;
    const wchar_t* expected_filename;
  } test_cases[] = {
      // Known extensions, with or without associated MIME types, should not get
      // an extension appended.
      {L"sample.html", L"*.txt", L"txt", L"sample.html"},
      {L"sample.reg", L"*.txt", L"txt", L"sample.reg"},

      // An unknown extension, or no extension, should get the default extension
      // appended.
      {L"sample.unknown", L"*.txt", L"txt", L"sample.unknown.txt"},
      {L"sample", L"*.txt", L"txt", L"sample.txt"},
      // ...unless the unknown and default extensions match.
      {L"sample.unknown", L"*.unknown", L"unknown", L"sample.unknown"},

      // The extension alone should be treated like a filename with no
      // extension.
      {L"txt", L"*.txt", L"txt", L"txt.txt"},

      // Trailing dots should cause us to append an extension.
      {L"sample.txt.", L"*.txt", L"txt", L"sample.txt.txt"},
      {L"...", L"*.txt", L"txt", L"...txt"},

      // If the filter is changed to "All files", we allow any filename.
      {L"sample.unknown", L"*.*", L"", L"sample.unknown"},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%zu", i));

    EXPECT_EQ(base::string16(test_cases[i].expected_filename),
              ui::AppendExtensionIfNeeded(test_cases[i].filename,
                                          test_cases[i].filter_selected,
                                          test_cases[i].suggested_ext));
  }
}
