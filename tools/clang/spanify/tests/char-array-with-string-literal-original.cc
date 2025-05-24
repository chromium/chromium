// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <uchar.h>
#include <wchar.h>

#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

void func() {
  // Expected rewrite:
  // const std::string_view buf = "123456789";
  const char buf[] = "123456789";
  std::ignore = buf[UnsafeIndex()];

  // Expected rewrite:
  // const std::wstring_view buf = L"123456789";
  const wchar_t buf2[] = L"123456789";
  std::ignore = buf2[UnsafeIndex()];

  // Expected rewrite:
  // const std::u16string_view buf = u"123456789";
  const char16_t buf4[] = u"123456789";
  std::ignore = buf4[UnsafeIndex()];

  // Expected rewrite:
  // const std::u32string_view buf = U"123456789";
  const char32_t buf5[] = U"123456789";
  std::ignore = buf5[UnsafeIndex()];
}

void non_const_case() {
  // The explicit specification of the element type and size are necessary,
  // otherwise the deduced element type will be const.
  //
  // Expected rewrite:
  // std::array<char, 4> buf{"abc"};
  char buf[] = "abc";
  std::ignore = buf[UnsafeIndex()];

  // Expected rewrite:
  // std::array<char, 1 + 3> buf2{"abc"};
  char buf2[1 + 3] = "abc";
  std::ignore = buf2[UnsafeIndex()];
}
