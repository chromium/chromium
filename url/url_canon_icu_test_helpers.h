// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_ICU_TEST_HELPERS_H_
#define URL_URL_CANON_ICU_TEST_HELPERS_H_

#include "base/logging.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "url/url_canon.h"

namespace url::test {

// Wrapper around a UConverter object that managers creation and destruction.
class UConvScoper {
 public:
  explicit UConvScoper(const char* charset_name) {
    UErrorCode err = U_ZERO_ERROR;
    converter_ = ucnv_open(charset_name, &err);
    if (!converter_) {
      LOG(ERROR) << "Failed to open charset " << charset_name << ": "
                 << u_errorName(err);
    }
  }

  ~UConvScoper() {
    if (converter_) {
      ucnv_close(converter_.ExtractAsDangling());
    }
  }

  // Returns the converter object, may be NULL.
  UConverter* converter() const { return converter_; }

 private:
  raw_ptr<UConverter> converter_;
};

}  // namespace url::test

#endif  // URL_URL_CANON_ICU_TEST_HELPERS_H_
