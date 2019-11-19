// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ICU-based IDNA converter.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "url/url_canon_icu.h"
#include "url/url_canon_internal.h"  // for _itoa_s

namespace url {

namespace {

// A wrapper to use LazyInstance<>::Leaky with ICU's UIDNA, a C pointer to
// a UTS46/IDNA 2008 handling object opened with uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 (with unassigned
// code points allowed) to IDNA 2008 with
// the backward compatibility in mind. What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as
//    in IDNA 2003.
// 3. Use transitional mechanism for 4 deviation characters (sharp-s,
//    final sigma, ZWJ and ZWNJ) for now.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein
// for more details.
struct UIDNAWrapper {
  UIDNAWrapper() {
    UErrorCode err = U_ZERO_ERROR;
    // TODO(jungshik): Change options as different parties (browsers,
    // registrars, search engines) converge toward a consensus.
    value = uidna_openUTS46(UIDNA_CHECK_BIDI, &err);
    if (U_FAILURE(err)) {
      CHECK(false) << "failed to open UTS46 data with error: "
                   << u_errorName(err)
                   << ". If you see this error message in a test environment "
                   << "your test environment likely lacks the required data "
                   << "tables for libicu. See https://crbug.com/778929.";
      value = NULL;
    }
  }

  UIDNA* value;
};

}  // namespace

UIDNA* GetUIDNA() {
  static base::NoDestructor<UIDNAWrapper> uidna_wrapper;
  return uidna_wrapper->value;
}

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
bool IDNToASCII(const base::char16* src, int src_len, CanonOutputW* output) {
  DCHECK(output->length() == 0);  // Output buffer is assumed empty.

  UIDNA* uidna = GetUIDNA();
  DCHECK(uidna != NULL);
  while (true) {
    UErrorCode err = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    int output_length = uidna_nameToASCII(uidna, src, src_len, output->data(),
                                          output->capacity(), &info, &err);
    if (U_SUCCESS(err) && info.errors == 0) {
      output->set_length(output_length);
      return true;
    }

    // TODO(jungshik): Look at info.errors to handle them case-by-case basis
    // if necessary.
    if (err != U_BUFFER_OVERFLOW_ERROR || info.errors != 0)
      return false;  // Unknown error, give up.

    // Not enough room in our buffer, expand.
    output->Resize(output_length);
  }
}

}  // namespace url
