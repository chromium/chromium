/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_REFERENCE_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_REFERENCE_H_

#include <cstddef>
#include <cstdint>
#include <utility>

namespace art {

class DexFile;

class DexFileReference {
 public:
  DexFileReference(const DexFile* file, uint32_t idx) : dex_file(file), index(idx) {}
  const DexFile* dex_file;
  uint32_t index;

  struct Comparator {
    bool operator()(const DexFileReference& a, const DexFileReference& b) const {
      if (a.dex_file != b.dex_file) {
        return a.dex_file < b.dex_file;
      }
      return a.index < b.index;
    }
  };
};

// Default comparators, compares the indices, not the backing data.
inline bool operator<(const DexFileReference& a, const DexFileReference& b) {
  return DexFileReference::Comparator()(a, b);
}
inline bool operator==(const DexFileReference& a, const DexFileReference& b) {
  return a.dex_file == b.dex_file && a.index == b.index;
}
inline bool operator!=(const DexFileReference& a, const DexFileReference& b) {
  return !(a == b);
}

}  // namespace art

namespace std {
// Hash implementation used for std::set/std::map. Simply xor the hash of the dex-file and index
template <>
struct hash<art::DexFileReference> {
  size_t operator()(const art::DexFileReference& ref) const {
    return hash<decltype(ref.dex_file)>()(ref.dex_file) ^
           hash<decltype(ref.index)>()(ref.index);
  }
};
}  // namespace std

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_REFERENCE_H_
