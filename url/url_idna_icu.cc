// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ICU-based IDNA converter.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "url/url_canon_icu.h"
#include "url/url_canon_internal.h"  // for _itoa_s
#include "url/url_features.h"

namespace url {

namespace {

// Use UIDNA, a C pointer to a UTS46/IDNA 2008 handling object opened with
// uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 (with unassigned
// code points allowed) to IDNA 2008 with the backward compatibility in mind.
// What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as
//    in IDNA 2003.
// 3. If `use_idna_non_transitional` is true, use non-transitional mechanism for
//    4 deviation characters (sharp-s, final sigma, ZWJ and ZWNJ) per
//    url.spec.whatwg.org.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein
// for more details.
UIDNA* CreateIDNA(bool use_idna_non_transitional) {
  uint32_t options = UIDNA_CHECK_BIDI;
  if (use_idna_non_transitional) {
    // Use non-transitional processing if enabled. See
    // https://url.spec.whatwg.org/#idna for details.
    options |=
        UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE;
  }
  UErrorCode err = U_ZERO_ERROR;
  UIDNA* idna = uidna_openUTS46(options, &err);
  if (U_FAILURE(err)) {
    CHECK(false) << "failed to open UTS46 data with error: " << u_errorName(err)
                 << ". If you see this error message in a test environment "
                 << "your test environment likely lacks the required data "
                 << "tables for libicu. See https://crbug.com/778929.";
    idna = nullptr;
  }
  return idna;
}

UIDNA* GetUIDNA() {
  // This logic results in having two UIDNA instances in tests. This is okay.
  if (IsUsingIDNA2008NonTransitional()) {
    static UIDNA* uidna = CreateIDNA(/*use_idna_non_transitional=*/true);
    return uidna;
  } else {
    static UIDNA* uidna = CreateIDNA(/*use_idna_non_transitional=*/false);
    return uidna;
  }
}

}  // namespace

// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must be ASCII, but is represented as wide characters.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, this will return false. The output in this case is undefined.
// TODO(jungshik): use UTF-8/ASCII version of nameToASCII.
// Change the function signature and callers accordingly to avoid unnecessary
// conversions in our code. In addition, consider using icu::IDNA's UTF-8/ASCII
// version with StringByteSink. That way, we can avoid C wrappers and additional
// string conversion.
bool IDNToASCII(std::u16string_view src, CanonOutputW* output) {
  DCHECK(output->length() == 0);  // Output buffer is assumed empty.

  UIDNA* uidna = GetUIDNA();
  DCHECK(uidna != nullptr);
  while (true) {
    UErrorCode err = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    int output_length = uidna_nameToASCII(
        uidna, src.data(), base::checked_cast<int32_t>(src.size()),
        output->data(), output->capacity(), &info, &err);

    // Ignore various errors for web compatibility. The options are specified
    // by the WHATWG URL Standard. See
    //  - https://unicode.org/reports/tr46/
    //  - https://url.spec.whatwg.org/#concept-domain-to-ascii
    //    (we set beStrict to false)

    // Disable the "CheckHyphens" option in UTS #46. See
    //  - https://crbug.com/804688
    //  - https://github.com/whatwg/url/issues/267
    info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
    info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
    info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

    // Disable the "VerifyDnsLength" option in UTS #46.
    info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
    info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
    info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;

    if (U_SUCCESS(err) && info.errors == 0) {
      // Per WHATWG URL, it is a failure if the ToASCII output is empty.
      //
      // ICU would usually return UIDNA_ERROR_EMPTY_LABEL in this case, but we
      // want to continue allowing http://abc..def/ while forbidding http:///.
      //
      if (output_length == 0) {
        return false;
      }

      output->set_length(output_length);
      return true;
    }

    if (err != U_BUFFER_OVERFLOW_ERROR || info.errors != 0)
      return false;  // Unknown error, give up.

    // Not enough room in our buffer, expand.
    output->Resize(output_length);
  }
}

}  // namespace url
