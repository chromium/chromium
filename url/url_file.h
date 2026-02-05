// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_FILE_H_
#define URL_URL_FILE_H_

// Provides shared functions used by the internals of the parser and
// canonicalizer for file URLs. Do not use outside of these modules.

#include "base/strings/string_util.h"
#include "url/url_parse_internal.h"

namespace url {

// We allow both "c:" and "c|" as drive identifiers.
inline bool IsWindowsDriveSeparator(char16_t ch) {
  return ch == ':' || ch == '|';
}
inline bool IsWindowsDriveSeparator(char ch) {
  return IsWindowsDriveSeparator(static_cast<char16_t>(ch));
}

// DoesContainWindowsDriveSpecUntil returns the least number between
// start_offset and max_offset such that the spec has a valid drive
// specification starting at that offset. Otherwise it returns `npos`. This
// function gracefully handles, by returning `npos`, start_offset values that
// are equal to or larger than the spec.length(), and caps max_offset
// appropriately to simplify callers. max_offset must be at least start_offset.
template <typename CHAR>
inline size_t DoesContainWindowsDriveSpecUntil(
    std::basic_string_view<CHAR> spec,
    size_t start_offset,
    size_t max_offset) {
  CHECK_LE(start_offset, max_offset);
  size_t spec_len = spec.length();
  if (spec_len < 2 || start_offset > spec_len - 2) {
    return std::basic_string_view<CHAR>::npos;  // Not enough room.
  }
  if (max_offset > spec_len - 2)
    max_offset = spec_len - 2;
  for (size_t offset = start_offset; offset <= max_offset; ++offset) {
    if (!base::IsAsciiAlpha(spec[offset])) {
      continue;  // Doesn't contain a valid drive letter.
    }
    if (!IsWindowsDriveSeparator(spec[offset + 1])) {
      continue;  // Isn't followed with a drive separator.
    }
    return offset;
  }
  return std::basic_string_view<CHAR>::npos;
}

// Returns true if the start_offset in the given spec looks like it begins a
// drive spec, for example "c:". This function explicitly handles start_offset
// values that are equal to or larger than the spec_len to simplify callers.
//
// If this returns true, the spec is guaranteed to have a valid drive letter
// plus a drive letter separator (a colon or a pipe) starting at |start_offset|.
template <typename CHAR>
inline bool DoesBeginWindowsDriveSpec(std::basic_string_view<CHAR> spec,
                                      size_t start_offset) {
  return DoesContainWindowsDriveSpecUntil(spec, start_offset, start_offset) ==
         start_offset;
}

#ifdef WIN32

// Returns true if the start_offset in the given text looks like it begins a
// UNC path, for example "\\". This function explicitly handles start_offset
// values that are equal to or larger than the spec_len to simplify callers.
//
// When strict_slashes is set, this function will only accept backslashes as is
// standard for Windows. Otherwise, it will accept forward slashes as well
// which we use for a lot of URL handling.
template <typename CHAR>
inline bool DoesBeginUncPath(std::basic_string_view<CHAR> text,
                             size_t start_offset,
                             bool strict_slashes) {
  if (start_offset >= text.length() || text.length() - start_offset < 2) {
    return false;
  }

  CHAR ch0 = text[start_offset];
  CHAR ch1 = text[++start_offset];
  if (strict_slashes) {
    return ch0 == '\\' && ch1 == '\\';
  }
  return IsSlashOrBackslash(ch0) && IsSlashOrBackslash(ch1);
}

#endif  // WIN32

}  // namespace url

#endif  // URL_URL_FILE_H_
