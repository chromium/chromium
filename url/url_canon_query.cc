// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"

// Query canonicalization in IE
// ----------------------------
// IE is very permissive for query parameters specified in links on the page
// (in contrast to links that it constructs itself based on form data). It does
// not unescape any character. It does not reject any escape sequence (be they
// invalid like "%2y" or freaky like %00).
//
// IE only escapes spaces and nothing else. Embedded NULLs, tabs (0x09),
// LF (0x0a), and CR (0x0d) are removed (this probably happens at an earlier
// layer since they are removed from all portions of the URL). All other
// characters are passed unmodified. Invalid UTF-16 sequences are preserved as
// well, with each character in the input being converted to UTF-8. It is the
// server's job to make sense of this invalid query.
//
// Invalid multibyte sequences (for example, invalid UTF-8 on a UTF-8 page)
// are converted to the invalid character and sent as unescaped UTF-8 (0xef,
// 0xbf, 0xbd). This may not be canonicalization, the parser may generate these
// strings before the URL handler ever sees them.
//
// Our query canonicalization
// --------------------------
// We escape all non-ASCII characters and control characters, like Firefox.
// This is more conformant to the URL spec, and there do not seem to be many
// problems relating to Firefox's behavior.
//
// Like IE, we will never unescape (although the application may want to try
// unescaping to present the user with a more understandable URL). We will
// replace all invalid sequences (including invalid UTF-16 sequences, which IE
// doesn't) with the "invalid character," and we will escape it.

namespace url {

namespace {

// Appends the given string to the output, escaping characters that do not
// match the given |type| in SharedCharTypes. This version will accept 8 or 16
// bit characters, but assumes that they have only 7-bit values. It also assumes
// that all UTF-8 values are correct, so doesn't bother checking
template <typename CHAR>
void AppendRaw8BitQueryString(std::basic_string_view<CHAR> source,
                              CanonOutput* output) {
  for (const CHAR& c : source) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!IsQueryChar(uc)) {
      AppendEscapedChar(uc, output);
    } else {  // Doesn't need escaping.
      output->push_back(static_cast<char>(uc));
    }
  }
}

// Runs the converter on the given UTF-8 input. Since the converter expects
// UTF-16, we have to convert first. The converter must be non-NULL.
void RunConverter(std::string_view input,
                  CharsetConverter* converter,
                  CanonOutput* output) {
  // This function will replace any misencoded values with the invalid
  // character. This is what we want so we don't have to check for error.
  RawCanonOutputW<1024> utf16;
  ConvertUTF8ToUTF16(input, &utf16);
  converter->ConvertFromUTF16(utf16.view(), output);
}

// Runs the converter with the given UTF-16 input. We don't have to do
// anything, but this overridden function allows us to use the same code
// for both UTF-8 and UTF-16 input.
void RunConverter(std::u16string_view input,
                  CharsetConverter* converter,
                  CanonOutput* output) {
  converter->ConvertFromUTF16(input, output);
}

template <typename CHAR, typename UCHAR>
void DoConvertToQueryEncoding(std::basic_string_view<CHAR> input,
                              CharsetConverter* converter,
                              CanonOutput* output) {
  if (converter) {
    // Run the converter to get an 8-bit string, then append it, escaping
    // necessary values.
    RawCanonOutput<1024> eight_bit;
    RunConverter(input, converter, &eight_bit);
    AppendRaw8BitQueryString(eight_bit.view(), output);

  } else {
    // No converter, do our own UTF-8 conversion.
    AppendStringOfType(input, CHAR_QUERY, output);
  }
}

template <typename CHAR, typename UCHAR>
void DoCanonicalizeQuery(std::optional<std::basic_string_view<CHAR>> input,
                         CharsetConverter* converter,
                         CanonOutput* output,
                         Component* out_query) {
  if (!input.has_value()) {
    *out_query = Component();
    return;
  }

  auto input_value = input.value();

  output->push_back('?');
  out_query->begin = output->length();

  DoConvertToQueryEncoding<CHAR, UCHAR>(input_value, converter, output);

  out_query->len = output->length() - out_query->begin;
}

}  // namespace

void CanonicalizeQuery(std::optional<std::string_view> input,
                       CharsetConverter* converter,
                       CanonOutput* output,
                       Component* out_query) {
  DoCanonicalizeQuery<char, unsigned char>(input, converter, output, out_query);
}

void CanonicalizeQuery(std::optional<std::u16string_view> input,
                       CharsetConverter* converter,
                       CanonOutput* output,
                       Component* out_query) {
  DoCanonicalizeQuery<char16_t, char16_t>(input, converter, output, out_query);
}

void ConvertUTF16ToQueryEncoding(std::u16string_view input,
                                 CharsetConverter* converter,
                                 CanonOutput* output) {
  DoConvertToQueryEncoding<char16_t, char16_t>(input, converter, output);
}

}  // namespace url
