/*
 * Copyright 2021 Google LLC
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

#ifndef MALDOCA_OLE_STRINGS_EXTRACT_H_
#define MALDOCA_OLE_STRINGS_EXTRACT_H_

#include <set>

#include "absl/strings/string_view.h"

namespace maldoca {

// Checks if the string is interesting (from the string extraction point of
// view). Interesting string in that case is:
// - alphanumeric characters consitute 75% of all characters
// NOTE:
// this function doesn't check if the characters are printable, since it assumes
// that it was checked during string extraction.
bool IsInterestingString(absl::string_view s);
void GetStrings(absl::string_view s, int min_len,
                std::set<std::string> *strings);

}  // namespace maldoca

#endif  // MALDOCA_OLE_STRINGS_EXTRACT_H_
