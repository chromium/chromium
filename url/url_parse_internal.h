// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_PARSE_INTERNAL_H_
#define URL_URL_PARSE_INTERNAL_H_

// Contains common inline helper functions used by the URL parsing routines.

#include "url/third_party/mozilla/url_parse.h"

namespace url {

// A helper function to handle a URL separator, which is '/' or '\'.
//
// The motivation: There are many condition checks in URL Standard like the
// following:
//
// > If url is special and c is U+002F (/) or U+005C (\), ...
inline bool IsSlashOrBackslash(char16_t ch) {
  return ch == '/' || ch == '\\';
}
inline bool IsSlashOrBackslash(char ch) {
  return IsSlashOrBackslash(static_cast<char16_t>(ch));
}

// Returns true if we should trim this character from the URL because it is a
// space or a control character.
inline bool ShouldTrimFromURL(char16_t ch) {
  return ch <= ' ';
}
inline bool ShouldTrimFromURL(char ch) {
  return ShouldTrimFromURL(static_cast<char16_t>(ch));
}

// This shrinks the input URL string to eliminate "should-be-trimmed"
// characters. The returned value is a pair of the start index of the remaining
// string and the start index of the trailing trimmed string in `spec`.
template <typename CHAR>
inline std::pair<size_t, size_t> TrimUrl(std::basic_string_view<CHAR> spec,
                                         bool trim_path_end = true) {
  size_t begin = 0;
  size_t end = spec.length();
  // Strip leading whitespace and control characters.
  while (begin < end && ShouldTrimFromURL(spec[begin])) {
    ++begin;
  }

  if (trim_path_end) {
    // Strip trailing whitespace and control characters. We need the `begin <
    // end` test for when the input string is all blanks.
    while (begin < end && ShouldTrimFromURL(spec[end - 1])) {
      --end;
    }
  }
  return {begin, end};
}

// Counts the number of consecutive slashes or backslashes starting at the given
// offset in the given string of the given length. A slash and backslash can be
// mixed.
template <typename CHAR>
inline size_t CountConsecutiveSlashesOrBackslashes(
    std::basic_string_view<CHAR> str,
    size_t begin_offset) {
  size_t count = 0;
  while (begin_offset < str.length() &&
         IsSlashOrBackslash(str[begin_offset++])) {
    ++count;
  }
  return count;
}

// Returns true if char is a slash.
inline bool IsSlash(char16_t ch) {
  return ch == '/';
}
inline bool IsSlash(char ch) {
  return IsSlash(static_cast<char16_t>(ch));
}

// Counts the number of consecutive slashes starting at the given offset
// in the given string of the given length.
template <typename CHAR>
inline size_t CountConsecutiveSlashes(std::basic_string_view<CHAR> str,
                                      size_t begin_offset) {
  size_t count = 0;
  while (begin_offset + count < str.length() &&
         IsSlash(str[begin_offset + count])) {
    ++count;
  }
  return count;
}

// Internal functions in url_parse.cc that parse the path, that is, everything
// following the authority section. The input is the range of everything
// following the authority section, and the output is the identified ranges.
//
// This is designed for the file URL parser or other consumers who may do
// special stuff at the beginning, but want regular path parsing, it just
// maps to the internal parsing function for paths.
void ParsePathInternal(std::string_view spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);
void ParsePathInternal(std::u16string_view spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);

// Internal functions in url_parse.cc that parse non-special URLs, which are
// similar to `ParseNonSpecialUrl` functions in url_parse.h, but with
// `trim_path_end` parameter that controls whether to trim path end or not.
Parsed ParseNonSpecialUrlInternal(std::string_view url, bool trim_path_end);
Parsed ParseNonSpecialUrlInternal(std::u16string_view url, bool trim_path_end);

// Given a spec and a pointer to the character after the colon following the
// special scheme, this parses it and fills in the structure, Every item in the
// parsed structure is filled EXCEPT for the scheme, which is untouched.
void ParseAfterSpecialScheme(std::string_view spec,
                             int after_scheme,
                             Parsed* parsed);
void ParseAfterSpecialScheme(std::u16string_view spec,
                             int after_scheme,
                             Parsed* parsed);

// Given a spec and a pointer to the character after the colon following the
// non-special scheme, this parses it and fills in the structure, Every item in
// the parsed structure is filled EXCEPT for the scheme, which is untouched.
void ParseAfterNonSpecialScheme(std::string_view spec,
                                int after_scheme,
                                Parsed* parsed);
void ParseAfterNonSpecialScheme(std::u16string_view spec,
                                int after_scheme,
                                Parsed* parsed);

}  // namespace url

#endif  // URL_URL_PARSE_INTERNAL_H_
