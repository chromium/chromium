// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/canonicalize_string.h"

#include <stdint.h>

#include "base/check_op.h"
#include "third_party/icu/source/common/unicode/errorcode.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/util/scoped_ptr.h"

namespace i18n {
namespace addressinput {

namespace {

class ChromeStringCanonicalizer : public StringCanonicalizer {
 public:
  ChromeStringCanonicalizer()
      : error_code_(U_ZERO_ERROR),
        collator_(
            icu::Collator::createInstance(
                icu::Locale::getRoot(), error_code_)) {
    collator_->setStrength(icu::Collator::PRIMARY);
    DCHECK(U_SUCCESS(error_code_));
  }

  ChromeStringCanonicalizer(const ChromeStringCanonicalizer&) = delete;
  ChromeStringCanonicalizer& operator=(const ChromeStringCanonicalizer&) =
      delete;

  virtual ~ChromeStringCanonicalizer() {}

  // StringCanonicalizer implementation.
  virtual std::string CanonicalizeString(const std::string& original) {
    // Returns a canonical version of the string that can be used for comparing
    // strings regardless of diacritics and capitalization.
    //    CanonicalizeString("Texas") == CanonicalizeString("T\u00E9xas");
    //    CanonicalizeString("Texas") == CanonicalizeString("teXas");
    //    CanonicalizeString("Texas") != CanonicalizeString("California");
    //
    // The output is not human-readable.
    //    CanonicalizeString("Texas") != "Texas";
    icu::UnicodeString icu_str(
        original.c_str(), static_cast<int32_t>(original.length()));
    int32_t buffer_size = collator_->getSortKey(icu_str, NULL, 0);
    scoped_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
    DCHECK(buffer.get());
    int32_t filled_size =
        collator_->getSortKey(icu_str, buffer.get(), buffer_size);
    DCHECK_EQ(buffer_size, filled_size);
    return std::string(reinterpret_cast<const char*>(buffer.get()));
  }

 private:
  UErrorCode error_code_;
  scoped_ptr<icu::Collator> collator_;
};

}  // namespace

// static
scoped_ptr<StringCanonicalizer> StringCanonicalizer::Build() {
  return scoped_ptr<StringCanonicalizer>(new ChromeStringCanonicalizer);
}

}  // namespace addressinput
}  // namespace i18n
