// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ICU-based character set converter.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/check.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "third_party/icu/source/common/unicode/ucnv_cb.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "url/url_canon_icu.h"
#include "url/url_canon_internal.h"  // for _itoa_s

namespace url {

namespace {

// Called when converting a character that can not be represented, this will
// append an escaped version of the numerical character reference for that code
// point. It is of the form "&#1234;" and we will escape the non-digits to
// "%26%231234%3B". Why? This is what Netscape did back in the olden days.
void appendURLEscapedChar(const void* context,
                          UConverterFromUnicodeArgs* from_args,
                          const UChar* code_units,
                          int32_t length,
                          UChar32 code_point,
                          UConverterCallbackReason reason,
                          UErrorCode* err) {
  if (reason == UCNV_UNASSIGNED) {
    *err = U_ZERO_ERROR;

    const static int prefix_len = 6;
    const static char prefix[prefix_len + 1] = "%26%23";  // "&#" percent-escaped
    ucnv_cbFromUWriteBytes(from_args, prefix, prefix_len, 0, err);

    DCHECK(code_point < 0x110000);
    char number[8];  // Max Unicode code point is 7 digits.
    _itoa_s(code_point, number, 10);
    int number_len = static_cast<int>(strlen(number));
    ucnv_cbFromUWriteBytes(from_args, number, number_len, 0, err);

    const static int postfix_len = 3;
    const static char postfix[postfix_len + 1] = "%3B";   // ";" percent-escaped
    ucnv_cbFromUWriteBytes(from_args, postfix, postfix_len, 0, err);
  }
}

// A class for scoping the installation of the invalid character callback.
class AppendHandlerInstaller {
 public:
  // The owner of this object must ensure that the converter is alive for the
  // duration of this object's lifetime.
  AppendHandlerInstaller(UConverter* converter) : converter_(converter) {
    UErrorCode err = U_ZERO_ERROR;
    ucnv_setFromUCallBack(converter_, appendURLEscapedChar, 0,
                          &old_callback_, &old_context_, &err);
  }

  ~AppendHandlerInstaller() {
    UErrorCode err = U_ZERO_ERROR;
    ucnv_setFromUCallBack(converter_, old_callback_, old_context_, 0, 0, &err);
  }

 private:
  UConverter* converter_;

  UConverterFromUCallback old_callback_;
  const void* old_context_;
};

}  // namespace

ICUCharsetConverter::ICUCharsetConverter(UConverter* converter)
    : converter_(converter) {
}

ICUCharsetConverter::~ICUCharsetConverter() = default;

void ICUCharsetConverter::ConvertFromUTF16(const base::char16* input,
                                           int input_len,
                                           CanonOutput* output) {
  // Install our error handler. It will be called for character that can not
  // be represented in the destination character set.
  AppendHandlerInstaller handler(converter_);

  int begin_offset = output->length();
  int dest_capacity = output->capacity() - begin_offset;
  output->set_length(output->length());

  do {
    UErrorCode err = U_ZERO_ERROR;
    char* dest = &output->data()[begin_offset];
    int required_capacity = ucnv_fromUChars(converter_, dest, dest_capacity,
                                            input, input_len, &err);
    if (err != U_BUFFER_OVERFLOW_ERROR) {
      output->set_length(begin_offset + required_capacity);
      return;
    }

    // Output didn't fit, expand
    dest_capacity = required_capacity;
    output->Resize(begin_offset + dest_capacity);
  } while (true);
}

}  // namespace url
