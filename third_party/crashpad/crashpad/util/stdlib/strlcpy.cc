// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stdlib/strlcpy.h"

#include <string>

namespace crashpad {

size_t c16lcpy(base::char16* destination,
               const base::char16* source,
               size_t length) {
  size_t source_length = std::char_traits<base::char16>::length(source);
  if (source_length < length) {
    std::char_traits<base::char16>::copy(
        destination, source, source_length + 1);
  } else if (length != 0) {
    std::char_traits<base::char16>::copy(destination, source, length - 1);
    destination[length - 1] = '\0';
  }
  return source_length;
}

}  // namespace crashpad
