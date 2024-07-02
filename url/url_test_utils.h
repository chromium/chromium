// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef URL_URL_TEST_UTILS_H_
#define URL_URL_TEST_UTILS_H_

// Convenience functions for string conversions.
// These are mostly intended for use in unit tests.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_canon_internal.h"

namespace url {

namespace test_utils {

// Converts a UTF-16 string from native wchar_t format to char16 by
// truncating the high 32 bits. This is different than the conversion function
// in base bacause it passes invalid UTF-16 characters which is important for
// test purposes. As a result, this is not meant to handle true UTF-32 encoded
// strings.
inline std::u16string TruncateWStringToUTF16(const wchar_t* src) {
  std::u16string str;
  int length = static_cast<int>(wcslen(src));
  for (int i = 0; i < length; ++i) {
    str.push_back(static_cast<char16_t>(src[i]));
  }
  return str;
}

}  // namespace test_utils

}  // namespace url

#endif  // URL_URL_TEST_UTILS_H_
