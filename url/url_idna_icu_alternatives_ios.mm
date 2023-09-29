// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <ostream>
#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "url/url_canon_internal.h"

namespace url {

// Only allow ASCII to avoid ICU dependency. Use NSString+IDN
// to convert non-ASCII URL prior to passing to API.
bool IDNToASCII(const char16_t* src, int src_len, CanonOutputW* output) {
  if (base::IsStringASCII(std::u16string_view(src, src_len))) {
    output->Append(src, src_len);
    return true;
  }
  DCHECK(false) << "IDN URL support is not available.";
  return false;
}

}  // namespace url
