// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/strings_extract.h"

#include "absl/strings/ascii.h"

namespace maldoca {

namespace {
// Gets ascii string object from wide string inside string_view.
// It doesn't verify if this is an actual wide string, just skips null bytes,
// thus it is effectively "remove null bytes from string_view".
std::string GetWideString(absl::string_view s, int start, int end) {
  std::string ret;
  ret.reserve((end - start) / 2);
  for (auto c : s.substr(start, end - start)) {
    if (c) {
      ret += c;
    }
  }
  return ret;
}
}  // namespace

bool IsInterestingString(absl::string_view s) {
  // Minimum ratio of alphanumeric to other printable characters, it aims at
  // reducing number of strings that consist mostly of printable but not
  // alphanumeric characters. Those are usually not very interesting from the
  // "metadata" point of view, but we still want to keep http urls or other
  // strings where dash or semicolon happens sporadically. 0.75 was chosen after
  // checking output of a few files, totally not scientific method.
  constexpr float kMinAlnumRatio = 0.75;
  int alnum_counter = 0;
  for (auto c : s) {
    if (absl::ascii_isalnum(c)) {
      alnum_counter++;
    }
  }
  return (alnum_counter > kMinAlnumRatio * s.size());
}

void GetStrings(absl::string_view s, int min_len,
                std::set<std::string>* strings) {
  // gnu_strings from third_party requires 3 different calls to extract all
  // strings (k7Bit and twice k16BitLittleEndian for odd and even offsets) and
  // later filtering of unicode strings that I'm not interested in.
  // This version extracts all ASCII and Unicode strings in one pass and
  // includes only strings with printable ASCII characters (no diacritics).
  int first_printable = -1;
  int first_wide_printable = -1;
  int last_wide_printable = -1;
  for (int i = 0; i < s.size(); i++) {
    if (absl::ascii_isprint(s[i])) {
      // ascii strings
      if (first_printable < 0) {
        // "start" new ascii string
        first_printable = i;
      }
      // wide strings
      if ((i + 1 < s.size()) && (s[i + 1] == 0)) {  // is wide?
        last_wide_printable = i;
        if (first_wide_printable < 0) {
          // "start" new wide string
          first_wide_printable = i;
        }
      } else {
        // this is not a wide character, so output the wide string if conditions
        // are met
        if ((first_wide_printable >= 0) &&
            (i - first_wide_printable >= 2 * min_len)) {
          strings->emplace(GetWideString(s, first_wide_printable, i));
        }
        first_wide_printable = -1;
        last_wide_printable = -1;
      }
    } else {
      // handle non-printable characters
      // finish ascii string (if any)
      if ((first_printable >= 0) && (i - first_printable >= min_len)) {
        strings->emplace(s.substr(first_printable, i - first_printable));
      }
      first_printable = -1;
      // finish wide sring (if any)
      if (i != last_wide_printable + 1) {
        if ((first_wide_printable >= 0) &&
            (i - first_wide_printable >= 2 * min_len)) {
          strings->emplace(GetWideString(s, first_wide_printable, i));
        }
        first_wide_printable = -1;
        last_wide_printable = -1;
      }
    }
  }
  if (first_printable >= 0) {
    strings->emplace(s.substr(first_printable, s.size() - first_printable));
  }
  if (first_wide_printable >= 0) {
    strings->emplace(GetWideString(s, first_wide_printable, s.size()));
  }
}

}  // namespace maldoca
