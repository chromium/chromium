// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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

// Given an already-initialized begin index and length, this shrinks the range
// to eliminate "should-be-trimmed" characters. Note that the length does *not*
// indicate the length of untrimmed data from |*begin|, but rather the position
// in the input string (so the string starts at character |*begin| in the spec,
// and goes until |*len|).
template<typename CHAR>
inline void TrimURL(const CHAR* spec, int* begin, int* len,
                    bool trim_path_end = true) {
  // Strip leading whitespace and control characters.
  while (*begin < *len && ShouldTrimFromURL(spec[*begin]))
    (*begin)++;

  if (trim_path_end) {
    // Strip trailing whitespace and control characters. We need the >i test
    // for when the input string is all blanks; we don't want to back past the
    // input.
    while (*len > *begin && ShouldTrimFromURL(spec[*len - 1]))
      (*len)--;
  }
}

// Counts the number of consecutive slashes starting at the given offset
// in the given string of the given length.
template<typename CHAR>
inline int CountConsecutiveSlashes(const CHAR *str,
                                   int begin_offset, int str_len) {
  int count = 0;
  while (begin_offset + count < str_len &&
         IsSlashOrBackslash(str[begin_offset + count])) {
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
void ParsePathInternal(const char* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);
void ParsePathInternal(const char16_t* spec,
                       const Component& path,
                       Component* filepath,
                       Component* query,
                       Component* ref);

// Internal functions in url_parse.cc that parse non-special URLs, which are
// similar to `ParseNonSpecialURL` functions in url_parse.h, but with
// `trim_path_end` parameter that controls whether to trim path end or not.
Parsed ParseNonSpecialURLInternal(std::string_view url, bool trim_path_end);
Parsed ParseNonSpecialURLInternal(std::u16string_view url, bool trim_path_end);

// Given a spec and a pointer to the character after the colon following the
// special scheme, this parses it and fills in the structure, Every item in the
// parsed structure is filled EXCEPT for the scheme, which is untouched.
void ParseAfterSpecialScheme(const char* spec,
                             int spec_len,
                             int after_scheme,
                             Parsed* parsed);
void ParseAfterSpecialScheme(const char16_t* spec,
                             int spec_len,
                             int after_scheme,
                             Parsed* parsed);

// Given a spec and a pointer to the character after the colon following the
// non-special scheme, this parses it and fills in the structure, Every item in
// the parsed structure is filled EXCEPT for the scheme, which is untouched.
void ParseAfterNonSpecialScheme(const char* spec,
                                int spec_len,
                                int after_scheme,
                                Parsed* parsed);
void ParseAfterNonSpecialScheme(const char16_t* spec,
                                int spec_len,
                                int after_scheme,
                                Parsed* parsed);

}  // namespace url

#endif  // URL_URL_PARSE_INTERNAL_H_
