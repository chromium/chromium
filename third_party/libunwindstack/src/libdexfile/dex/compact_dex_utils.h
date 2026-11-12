/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_COMPACT_DEX_UTILS_H_
#define ART_LIBDEXFILE_DEX_COMPACT_DEX_UTILS_H_

#include <vector>

#include "base/bit_utils.h"

namespace art {

// Add padding to the end of the array until the size is aligned.
template <typename T, template<typename> class Allocator>
static inline void AlignmentPadVector(std::vector<T, Allocator<T>>* dest,
                                      size_t alignment) {
  while (!IsAlignedParam(dest->size(), alignment)) {
    dest->push_back(T());
  }
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_COMPACT_DEX_UTILS_H_
