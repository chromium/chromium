/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_
#define ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_

#include <string_view>

namespace art {

// When this code is only compiled on C++20+, these wrappers can be removed and
// calling code changed to call the string_view methods directly.

inline bool StartsWith(std::string_view sv, std::string_view prefix) {
#if !defined(__cpp_lib_starts_ends_with) || __cpp_lib_starts_ends_with < 201711L
  return sv.substr(0u, prefix.size()) == prefix;
#else
  return sv.starts_with(prefix);
#endif
}

inline bool EndsWith(std::string_view sv, std::string_view suffix) {
#if !defined(__cpp_lib_starts_ends_with) || __cpp_lib_starts_ends_with < 201711L
  return sv.size() >= suffix.size() && sv.substr(sv.size() - suffix.size()) == suffix;
#else
  return sv.ends_with(suffix);
#endif
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_STRING_VIEW_CPP20_H_
