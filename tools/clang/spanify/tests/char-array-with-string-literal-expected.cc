// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <uchar.h>
#include <wchar.h>

#include <string_view>

void func() {
  const int index = 0;
  // Expected rewrite:
  // const std::string_view buf = "123456789";
  const std::string_view buf = "123456789";
  (void)buf[index];

  // Expected rewrite:
  // const std::wstring_view buf = L"123456789";
  const std::wstring_view buf2 = L"123456789";
  (void)buf2[index];

  // Expected rewrite:
  // const std::u16string_view buf = u"123456789";
  const std::u16string_view buf4 = u"123456789";
  (void)buf4[index];

  // Expected rewrite:
  // const std::u32string_view buf = U"123456789";
  const std::u32string_view buf5 = U"123456789";
  (void)buf5[index];
}
