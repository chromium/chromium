// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_TEST_UTILS_H_
#define URL_URL_TEST_UTILS_H_

// Convenience functions for string conversions.
// These are mostly intended for use in unit tests.

#include <string>
#include <string_view>

namespace url::test_utils {

// Converts a UTF-16 string from native wchar_t format to char16 by
// truncating the high 32 bits. This is different than the conversion function
// in base bacause it passes invalid UTF-16 characters which is important for
// test purposes. As a result, this is not meant to handle true UTF-32 encoded
// strings.
inline std::u16string TruncateWStringToUtf16(std::wstring_view src) {
  std::u16string str;
  for (auto wchar : src) {
    str.push_back(static_cast<char16_t>(wchar));
  }
  return str;
}

}  // namespace url::test_utils

#endif  // URL_URL_TEST_UTILS_H_
