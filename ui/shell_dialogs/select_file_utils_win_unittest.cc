// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/shell_dialogs/select_file_utils_win.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SelectFileUtilsWin, RemoveEnvVarFromFileName) {
  struct RemoveEnvFromFileNameTestCase {
    const wchar_t* filename;
    const wchar_t* sanitized_filename;
  } test_cases[] = {
      {L"", L""},
      {L"a.txt", L"a.txt"},

      // Only 1 "%" in file name.
      {L"%", L"%"},
      {L"%.txt", L"%.txt"},
      {L"ab%c.txt", L"ab%c.txt"},
      {L"abc.t%", L"abc.t%"},

      // 2 "%" in file name.
      {L"%%", L""},
      {L"%c%", L""},
      {L"%c%d", L"d"},
      {L"d%c%.txt", L"d.txt"},
      {L"ab%c.t%", L"ab"},
      {L"abc.%t%", L"abc."},
      {L"*.%txt%", L"*."},

      // More than 2 "%" in file name.
      {L"%ab%c%.txt", L"c%.txt"},
      {L"%abc%.%txt%", L"."},
      {L"%ab%c%.%txt%", L"ctxt%"},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i=%zu", i));
    std::wstring sanitized = ui::RemoveEnvVarFromFileName<wchar_t>(
        std::wstring(test_cases[i].filename), std::wstring(L"%"));
    EXPECT_EQ(std::wstring(test_cases[i].sanitized_filename), sanitized);
  }
}
