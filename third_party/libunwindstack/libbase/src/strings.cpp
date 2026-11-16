/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "android-base/strings.h"

#include "android-base/stringprintf.h"

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

// Wraps the posix version of strerror_r to make it available in translation units
// that define _GNU_SOURCE.
extern "C" int posix_strerror_r(int errnum, char* buf, size_t buflen);

namespace android {
namespace base {

#define CHECK_NE(a, b) \
  if ((a) == (b)) abort();

std::vector<std::string> Split(const std::string& s,
                               const std::string& delimiters) {
  CHECK_NE(delimiters.size(), 0U);

  std::vector<std::string> result;

  size_t base = 0;
  size_t found;
  while (true) {
    found = s.find_first_of(delimiters, base);
    result.push_back(s.substr(base, found - base));
    if (found == s.npos) break;
    base = found + 1;
  }

  return result;
}

std::vector<std::string> Tokenize(const std::string& s, const std::string& delimiters) {
  CHECK_NE(delimiters.size(), 0U);

  std::vector<std::string> result;
  size_t end = 0;

  while (true) {
    size_t base = s.find_first_not_of(delimiters, end);
    if (base == s.npos) {
      break;
    }
    end = s.find_first_of(delimiters, base);
    result.push_back(s.substr(base, end - base));
  }
  return result;
}

template std::string Trim(const char*&);
template std::string Trim(const char*&&);
template std::string Trim(const std::string&);
template std::string Trim(const std::string&&);
template std::string Trim(std::string_view&);
template std::string Trim(std::string_view&&);

// These cases were measured either to be used during build by more than one binary, or during
// runtime as a significant portion of total calls.
// Instantiate them to aid compile time and binary size.
template std::string Join(std::vector<std::string>&, char);
template std::string Join(std::vector<std::string>&, const char*);
template std::string Join(std::vector<std::string>&&, const char*);
template std::string Join(const std::vector<std::string>&, char);
template std::string Join(const std::vector<std::string>&, const char*);
template std::string Join(const std::vector<std::string>&&, const char*);
template std::string Join(std::set<std::string>&, const char*);
template std::string Join(const std::set<std::string>&, char);
template std::string Join(const std::set<std::string>&, const char*);
template std::string Join(const std::unordered_set<std::string>&, const char*);


bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.starts_with(prefix);
}

bool StartsWith(std::string_view s, char prefix) {
  return s.starts_with(prefix);
}

bool StartsWithIgnoreCase(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && strncasecmp(s.data(), prefix.data(), prefix.size()) == 0;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
  return s.ends_with(suffix);
}

bool EndsWith(std::string_view s, char suffix) {
  return s.ends_with(suffix);
}

bool EndsWithIgnoreCase(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() &&
         strncasecmp(s.data() + (s.size() - suffix.size()), suffix.data(), suffix.size()) == 0;
}

bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) {
  return lhs.size() == rhs.size() && strncasecmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

std::string StringReplace(std::string_view s, std::string_view from, std::string_view to,
                          bool all) {
  if (from.empty()) return std::string(s);

  std::string result;
  std::string_view::size_type start_pos = 0;
  do {
    std::string_view::size_type pos = s.find(from, start_pos);
    if (pos == std::string_view::npos) break;

    result.append(s.data() + start_pos, pos - start_pos);
    result.append(to.data(), to.size());

    start_pos = pos + from.size();
  } while (all);
  result.append(s.data() + start_pos, s.size() - start_pos);
  return result;
}

std::string ErrnoNumberAsString(int errnum) {
  char buf[100];
  buf[0] = '\0';
  int strerror_err = posix_strerror_r(errnum, buf, sizeof(buf));
  if (strerror_err < 0) {
    return StringPrintf("Failed to convert errno %d to string: %d", errnum, strerror_err);
  }
  return buf;
}

}  // namespace base
}  // namespace android
