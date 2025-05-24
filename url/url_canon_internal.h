// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_CANON_INTERNAL_H_
#define URL_URL_CANON_INTERNAL_H_

// This file is intended to be included in another C++ file where the character
// types are defined. This allows us to write mostly generic code, but not have
// template bloat because everything is inlined when anybody calls any of our
// functions.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <array>
#include <string>

#include "base/component_export.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "url/url_canon.h"

namespace url {

// Character type handling -----------------------------------------------------

// Bits that identify different character types. These types identify different
// bits that are set for each 8-bit character in the kSharedCharTypeTable.
enum SharedCharTypes {
  // Characters that do not require escaping in queries. Characters that do
  // not have this flag will be escaped; see url_canon_query.cc
  CHAR_QUERY = 1,

  // Valid in the username/password field.
  CHAR_USERINFO = 2,

  // Valid in a IPv4 address (digits plus dot and 'x' for hex).
  CHAR_IPV4 = 4,

  // Valid in an ASCII-representation of a hex digit (as in %-escaped).
  CHAR_HEX = 8,

  // Valid in an ASCII-representation of a decimal digit.
  CHAR_DEC = 16,

  // Valid in an ASCII-representation of an octal digit.
  CHAR_OCT = 32,

  // Characters that do not require escaping in encodeURIComponent. Characters
  // that do not have this flag will be escaped; see url_util.cc.
  CHAR_COMPONENT = 64,
};

// This table contains the flags in SharedCharTypes for each 8-bit character.
// Some canonicalization functions have their own specialized lookup table.
// For those with simple requirements, we have collected the flags in one
// place so there are fewer lookup tables to load into the CPU cache.
//
// Using an unsigned char type has a small but measurable performance benefit
// over using a 32-bit number.
// clang-format off
inline constexpr std::array<uint8_t, 0x100> kSharedCharTypeTable = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x00 - 0x0f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x10 - 0x1f
    0,                           // 0x20  ' ' (escape spaces in queries)
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x21  !
    0,                           // 0x22  "
    0,                           // 0x23  #  (invalid in query since it marks the ref)
    CHAR_QUERY | CHAR_USERINFO,  // 0x24  $
    CHAR_QUERY | CHAR_USERINFO,  // 0x25  %
    CHAR_QUERY | CHAR_USERINFO,  // 0x26  &
    0,                           // 0x27  '  (Try to prevent XSS.)
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x28  (
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x29  )
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x2a  *
    CHAR_QUERY | CHAR_USERINFO,  // 0x2b  +
    CHAR_QUERY | CHAR_USERINFO,  // 0x2c  ,
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x2d  -
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_COMPONENT,  // 0x2e  .
    CHAR_QUERY,                  // 0x2f  /
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x30  0
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x31  1
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x32  2
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x33  3
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x34  4
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x35  5
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x36  6
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_OCT | CHAR_COMPONENT,  // 0x37  7
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_COMPONENT,             // 0x38  8
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_DEC | CHAR_COMPONENT,             // 0x39  9
    CHAR_QUERY,  // 0x3a  :
    CHAR_QUERY,  // 0x3b  ;
    0,           // 0x3c  <  (Try to prevent certain types of XSS.)
    CHAR_QUERY,  // 0x3d  =
    0,           // 0x3e  >  (Try to prevent certain types of XSS.)
    CHAR_QUERY,  // 0x3f  ?
    CHAR_QUERY,  // 0x40  @
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x41  A
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x42  B
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x43  C
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x44  D
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x45  E
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x46  F
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x47  G
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x48  H
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x49  I
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4a  J
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4b  K
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4c  L
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4d  M
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4e  N
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x4f  O
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x50  P
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x51  Q
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x52  R
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x53  S
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x54  T
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x55  U
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x56  V
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x57  W
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_COMPONENT, // 0x58  X
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x59  Y
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x5a  Z
    CHAR_QUERY,  // 0x5b  [
    CHAR_QUERY,  // 0x5c  '\'
    CHAR_QUERY,  // 0x5d  ]
    CHAR_QUERY,  // 0x5e  ^
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x5f  _
    CHAR_QUERY,  // 0x60  `
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x61  a
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x62  b
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x63  c
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x64  d
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x65  e
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_HEX | CHAR_COMPONENT,  // 0x66  f
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x67  g
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x68  h
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x69  i
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6a  j
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6b  k
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6c  l
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6d  m
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6e  n
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x6f  o
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x70  p
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x71  q
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x72  r
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x73  s
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x74  t
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x75  u
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x76  v
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x77  w
    CHAR_QUERY | CHAR_USERINFO | CHAR_IPV4 | CHAR_COMPONENT,  // 0x78  x
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x79  y
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x7a  z
    CHAR_QUERY,  // 0x7b  {
    CHAR_QUERY,  // 0x7c  |
    CHAR_QUERY,  // 0x7d  }
    CHAR_QUERY | CHAR_USERINFO | CHAR_COMPONENT,  // 0x7e  ~
    0,           // 0x7f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x80 - 0x8f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0x90 - 0x9f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xa0 - 0xaf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xb0 - 0xbf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xc0 - 0xcf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xd0 - 0xdf
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xe0 - 0xef
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 0xf0 - 0xff
};
// clang-format on

// More readable wrappers around the character type lookup table.
constexpr bool IsCharOfType(unsigned char c, SharedCharTypes type) {
  return !!(kSharedCharTypeTable[c] & type);
}
constexpr bool IsQueryChar(unsigned char c) {
  return IsCharOfType(c, CHAR_QUERY);
}
constexpr bool IsIPv4Char(unsigned char c) {
  return IsCharOfType(c, CHAR_IPV4);
}
constexpr bool IsHexChar(unsigned char c) {
  return IsCharOfType(c, CHAR_HEX);
}
constexpr bool IsComponentChar(unsigned char c) {
  return IsCharOfType(c, CHAR_COMPONENT);
}

// Appends the given string to the output, escaping characters that do not
// match the given |type| in SharedCharTypes.
void AppendStringOfType(std::string_view source,
                        SharedCharTypes type,
                        CanonOutput* output);
void AppendStringOfType(std::u16string_view source,
                        SharedCharTypes type,
                        CanonOutput* output);

// This lookup table allows fast conversion between ASCII hex letters and their
// corresponding numerical value. The 8-bit range is divided up into 8
// regions of 0x20 characters each. Each of the three character types (numbers,
// uppercase, lowercase) falls into different regions of this range. The table
// contains the amount to subtract from characters in that range to get at
// the corresponding numerical value.
//
// See HexDigitToValue for the lookup.
extern const char kCharToHexLookup[8];

// Assumes the input is a valid hex digit! Call IsHexChar before using this.
inline int HexCharToValue(unsigned char c) {
  return c - kCharToHexLookup[c / 0x20];
}

// Indicates if the given character is a dot or dot equivalent, returning the
// number of characters taken by it. This will be one for a literal dot, 3 for
// an escaped dot. If the character is not a dot, this will return 0.
template <typename CHAR>
inline size_t IsDot(const CHAR* spec, size_t offset, size_t end) {
  if (spec[offset] == '.') {
    return 1;
  } else if (spec[offset] == '%' && offset + 3 <= end &&
             spec[offset + 1] == '2' &&
             (spec[offset + 2] == 'e' || spec[offset + 2] == 'E')) {
    // Found "%2e"
    return 3;
  }
  return 0;
}

// Returns the canonicalized version of the input character according to scheme
// rules. This is implemented alongside the scheme canonicalizer, and is
// required for relative URL resolving to test for scheme equality.
//
// Returns 0 if the input character is not a valid scheme character.
char CanonicalSchemeChar(char16_t ch);

// Write a single character, escaped, to the output. This always escapes: it
// does no checking that thee character requires escaping.
// Escaping makes sense only 8 bit chars, so code works in all cases of
// input parameters (8/16bit).
template <typename UINCHAR, typename OUTCHAR>
inline void AppendEscapedChar(UINCHAR ch, CanonOutputT<OUTCHAR>* output) {
  output->push_back('%');
  std::string hex;
  base::AppendHexEncodedByte(static_cast<uint8_t>(ch), hex);
  output->push_back(static_cast<OUTCHAR>(hex[0]));
  output->push_back(static_cast<OUTCHAR>(hex[1]));
}

// The character we'll substitute for undecodable or invalid characters.
extern const base_icu::UChar32 kUnicodeReplacementCharacter;

// UTF-8 functions ------------------------------------------------------------

// Reads one character in UTF-8 starting at |*begin| in |str|, places
// the decoded value into |*code_point|, and returns true on success.
// Otherwise, we'll return false and put the kUnicodeReplacementCharacter
// into |*code_point|.
//
// |*begin| will be updated to point to the last character consumed so it
// can be incremented in a loop and will be ready for the next character.
// (for a single-byte ASCII character, it will not be changed).
COMPONENT_EXPORT(URL)
bool ReadUTFCharLossy(const char* str,
                      size_t* begin,
                      size_t length,
                      base_icu::UChar32* code_point_out);

// Generic To-UTF-8 converter. This will call the given append method for each
// character that should be appended, with the given output method. Wrappers
// are provided below for escaped and non-escaped versions of this.
//
// The char_value must have already been checked that it's a valid Unicode
// character.
template <class Output, void Appender(unsigned char, Output*)>
inline void DoAppendUTF8(base_icu::UChar32 char_value, Output* output) {
  DCHECK(char_value >= 0);
  DCHECK(char_value <= 0x10FFFF);
  if (char_value <= 0x7f) {
    Appender(static_cast<unsigned char>(char_value), output);
  } else if (char_value <= 0x7ff) {
    // 110xxxxx 10xxxxxx
    Appender(static_cast<unsigned char>(0xC0 | (char_value >> 6)), output);
    Appender(static_cast<unsigned char>(0x80 | (char_value & 0x3f)), output);
  } else if (char_value <= 0xffff) {
    // 1110xxxx 10xxxxxx 10xxxxxx
    Appender(static_cast<unsigned char>(0xe0 | (char_value >> 12)), output);
    Appender(static_cast<unsigned char>(0x80 | ((char_value >> 6) & 0x3f)),
             output);
    Appender(static_cast<unsigned char>(0x80 | (char_value & 0x3f)), output);
  } else {
    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    Appender(static_cast<unsigned char>(0xf0 | (char_value >> 18)), output);
    Appender(static_cast<unsigned char>(0x80 | ((char_value >> 12) & 0x3f)),
             output);
    Appender(static_cast<unsigned char>(0x80 | ((char_value >> 6) & 0x3f)),
             output);
    Appender(static_cast<unsigned char>(0x80 | (char_value & 0x3f)), output);
  }
}

// Helper used by AppendUTF8Value below. We use an unsigned parameter so there
// are no funny sign problems with the input, but then have to convert it to
// a regular char for appending.
inline void AppendCharToOutput(unsigned char ch, CanonOutput* output) {
  output->push_back(static_cast<char>(ch));
}

// Writes the given character to the output as UTF-8. This does NO checking
// of the validity of the Unicode characters; the caller should ensure that
// the value it is appending is valid to append.
inline void AppendUTF8Value(base_icu::UChar32 char_value, CanonOutput* output) {
  DoAppendUTF8<CanonOutput, AppendCharToOutput>(char_value, output);
}

// Writes the given character to the output as UTF-8, escaping ALL
// characters (even when they are ASCII). This does NO checking of the
// validity of the Unicode characters; the caller should ensure that the value
// it is appending is valid to append.
inline void AppendUTF8EscapedValue(base_icu::UChar32 char_value,
                                   CanonOutput* output) {
  DoAppendUTF8<CanonOutput, AppendEscapedChar>(char_value, output);
}

// UTF-16 functions -----------------------------------------------------------

// Reads one character in UTF-16 starting at |*begin| in |str|, places
// the decoded value into |*code_point|, and returns true on success.
// Otherwise, we'll return false and put the kUnicodeReplacementCharacter
// into |*code_point|.
//
// |*begin| will be updated to point to the last character consumed so it
// can be incremented in a loop and will be ready for the next character.
// (for a single-16-bit-word character, it will not be changed).
COMPONENT_EXPORT(URL)
bool ReadUTFCharLossy(const char16_t* str,
                      size_t* begin,
                      size_t length,
                      base_icu::UChar32* code_point_out);

// Equivalent to U16_APPEND_UNSAFE in ICU but uses our output method.
inline void AppendUTF16Value(base_icu::UChar32 code_point,
                             CanonOutputT<char16_t>* output) {
  if (code_point > 0xffff) {
    output->push_back(static_cast<char16_t>((code_point >> 10) + 0xd7c0));
    output->push_back(static_cast<char16_t>((code_point & 0x3ff) | 0xdc00));
  } else {
    output->push_back(static_cast<char16_t>(code_point));
  }
}

// Escaping functions ---------------------------------------------------------

// Writes the given character to the output as UTF-8, escaped. Call this
// function only when the input is wide. Returns true on success. Failure
// means there was some problem with the encoding, we'll still try to
// update the |*begin| pointer and add a placeholder character to the
// output so processing can continue.
//
// We will append the character starting at ch[begin] with the buffer ch
// being |length|. |*begin| will be updated to point to the last character
// consumed (we may consume more than one for UTF-16) so that if called in
// a loop, incrementing the pointer will move to the next character.
//
// Every single output character will be escaped. This means that if you
// give it an ASCII character as input, it will be escaped. Some code uses
// this when it knows that a character is invalid according to its rules
// for validity. If you don't want escaping for ASCII characters, you will
// have to filter them out prior to calling this function.
//
// Assumes that ch[begin] is within range in the array, but does not assume
// that any following characters are.
inline bool AppendUTF8EscapedChar(const char16_t* str,
                                  size_t* begin,
                                  size_t length,
                                  CanonOutput* output) {
  // UTF-16 input. ReadUTFCharLossy will handle invalid characters for us and
  // give us the kUnicodeReplacementCharacter, so we don't have to do special
  // checking after failure, just pass through the failure to the caller.
  base_icu::UChar32 char_value;
  bool success = ReadUTFCharLossy(str, begin, length, &char_value);
  AppendUTF8EscapedValue(char_value, output);
  return success;
}

// Handles UTF-8 input. See the wide version above for usage.
inline bool AppendUTF8EscapedChar(const char* str,
                                  size_t* begin,
                                  size_t length,
                                  CanonOutput* output) {
  // ReadUTFCharLossy will handle invalid characters for us and give us the
  // kUnicodeReplacementCharacter, so we don't have to do special checking
  // after failure, just pass through the failure to the caller.
  base_icu::UChar32 ch;
  bool success = ReadUTFCharLossy(str, begin, length, &ch);
  AppendUTF8EscapedValue(ch, output);
  return success;
}

// URL Standard: https://url.spec.whatwg.org/#c0-control-percent-encode-set
template <typename CHAR>
bool IsInC0ControlPercentEncodeSet(CHAR ch) {
  return ch < 0x20 || ch > 0x7E;
}

// Given a '%' character at |*begin| in the string |spec|, this will decode
// the escaped value and put it into |*unescaped_value| on success (returns
// true). On failure, this will return false, and will not write into
// |*unescaped_value|.
//
// |*begin| will be updated to point to the last character of the escape
// sequence so that when called with the index of a for loop, the next time
// through it will point to the next character to be considered. On failure,
// |*begin| will be unchanged.
inline bool Is8BitChar(char c) {
  return true;  // this case is specialized to avoid a warning
}
inline bool Is8BitChar(char16_t c) {
  return c <= 255;
}

template <typename CHAR>
inline bool DecodeEscaped(const CHAR* spec,
                          size_t* begin,
                          size_t end,
                          unsigned char* unescaped_value) {
  if (*begin + 3 > end || !Is8BitChar(spec[*begin + 1]) ||
      !Is8BitChar(spec[*begin + 2])) {
    // Invalid escape sequence because there's not enough room, or the
    // digits are not ASCII.
    return false;
  }

  unsigned char first = static_cast<unsigned char>(spec[*begin + 1]);
  unsigned char second = static_cast<unsigned char>(spec[*begin + 2]);
  if (!IsHexChar(first) || !IsHexChar(second)) {
    // Invalid hex digits, fail.
    return false;
  }

  // Valid escape sequence.
  *unescaped_value = static_cast<unsigned char>((HexCharToValue(first) << 4) +
                                                HexCharToValue(second));
  *begin += 2;
  return true;
}

// Appends the given substring to the output, escaping "some" characters that
// it feels may not be safe. It assumes the input values are all contained in
// 8-bit although it allows any type.
//
// This is used in error cases to append invalid output so that it looks
// approximately correct. Non-error cases should not call this function since
// the escaping rules are not guaranteed!
void AppendInvalidNarrowString(const char* spec,
                               size_t begin,
                               size_t end,
                               CanonOutput* output);
void AppendInvalidNarrowString(const char16_t* spec,
                               size_t begin,
                               size_t end,
                               CanonOutput* output);

// Misc canonicalization helpers ----------------------------------------------

// Converts between UTF-8 and UTF-16, returning true on successful conversion.
// The output will be appended to the given canonicalizer output (so make sure
// it's empty if you want to replace).
//
// On invalid input, this will still write as much output as possible,
// replacing the invalid characters with the "invalid character". It will
// return false in the failure case, and the caller should not continue as
// normal.
COMPONENT_EXPORT(URL)
bool ConvertUTF16ToUTF8(std::u16string_view input, CanonOutput* output);
COMPONENT_EXPORT(URL)
bool ConvertUTF8ToUTF16(std::string_view input, CanonOutputT<char16_t>* output);

// Converts from UTF-16 to 8-bit using the character set converter. If the
// converter is NULL, this will use UTF-8.
void ConvertUTF16ToQueryEncoding(const char16_t* input,
                                 const Component& query,
                                 CharsetConverter* converter,
                                 CanonOutput* output);

// Applies the replacements to the given component source. The component source
// should be pre-initialized to the "old" base. That is, all pointers will
// point to the spec of the old URL, and all of the Parsed components will
// be indices into that string.
//
// The pointers and components in the |source| for all non-NULL strings in the
// |repl| (replacements) will be updated to reference those strings.
// Canonicalizing with the new |source| and |parsed| can then combine URL
// components from many different strings.
void SetupOverrideComponents(const char* base,
                             const Replacements<char>& repl,
                             URLComponentSource<char>* source,
                             Parsed* parsed);

// Like the above 8-bit version, except that it additionally converts the
// UTF-16 input to UTF-8 before doing the overrides.
//
// The given utf8_buffer is used to store the converted components. They will
// be appended one after another, with the parsed structure identifying the
// appropriate substrings. This buffer is a parameter because the source has
// no storage, so the buffer must have the same lifetime as the source
// parameter owned by the caller.
//
// THE CALLER MUST NOT ADD TO THE |utf8_buffer| AFTER THIS CALL. Members of
// |source| will point into this buffer, which could be invalidated if
// additional data is added and the CanonOutput resizes its buffer.
//
// Returns true on success. False means that the input was not valid UTF-16,
// although we will have still done the override with "invalid characters" in
// place of errors.
bool SetupUTF16OverrideComponents(const char* base,
                                  const Replacements<char16_t>& repl,
                                  CanonOutput* utf8_buffer,
                                  URLComponentSource<char>* source,
                                  Parsed* parsed);

// Implemented in url_canon_path.cc, these are required by the relative URL
// resolver as well, so we declare them here.
bool CanonicalizePartialPathInternal(const char* spec,
                                     const Component& path,
                                     size_t path_begin_in_output,
                                     CanonMode canon_mode,
                                     CanonOutput* output);
bool CanonicalizePartialPathInternal(const char16_t* spec,
                                     const Component& path,
                                     size_t path_begin_in_output,
                                     CanonMode canon_mode,
                                     CanonOutput* output);

// Find the position of a bona fide Windows drive letter in the given path. If
// no leading drive letter is found, -1 is returned. This function correctly
// treats /c:/foo and /./c:/foo as having drive letters, and /def/c:/foo as not
// having a drive letter.
//
// Exported for tests.
COMPONENT_EXPORT(URL)
int FindWindowsDriveLetter(const char* spec, int begin, int end);
COMPONENT_EXPORT(URL)
int FindWindowsDriveLetter(const char16_t* spec, int begin, int end);

// StringToUint64WithBase is implemented separately because std::strtoull (and
// its variants like _stroui64 on Windows) are not guaranteed to be constexpr,
// preventing their direct use in constant expressions.  This custom
// implementation provides a constexpr-friendly alternative for use in contexts
// where constant evaluation is required.
constexpr uint64_t StringToUint64WithBase(std::string_view str, uint8_t base) {
  uint64_t result = 0;

  for (const char digit : str) {
    int value = -1;

    if (digit >= '0' && digit <= '9') {
      value = digit - '0';
    } else if (digit >= 'A' && digit <= 'Z') {
      value = digit - 'A' + 10;
    } else if (digit >= 'a' && digit <= 'z') {
      value = digit - 'a' + 10;
    }

    if (value < 0 || value >= base) {
      break;  // Invalid character for the given base.
    }

    result = result * base + static_cast<uint64_t>(value);
  }

  return result;
}

#ifndef WIN32

// Implementations of Windows' int-to-string conversions
COMPONENT_EXPORT(URL)
int _itoa_s(int value, char* buffer, size_t size_in_chars, int radix);

// Secure template overloads for these functions
template <size_t N>
inline int _itoa_s(int value, char (&buffer)[N], int radix) {
  return _itoa_s(value, buffer, N, radix);
}

#endif  // WIN32

// The threshold we set to consider SIMD processing, in bytes; there is
// no deep theory here, it's just set empirically to a value that seems
// to be good. (We don't really know why there's a slowdown for zero;
// but a guess would be that there's no need in going into a complex loop
// with a lot of setup for a five-byte string.)
static constexpr int kMinimumLengthForSIMD = 50;

}  // namespace url

#endif  // URL_URL_CANON_INTERNAL_H_
