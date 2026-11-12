/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_TYPES_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_TYPES_H_

#include <functional>
#include <iosfwd>
#include <limits>
#include <utility>

namespace art {
namespace dex {

constexpr uint32_t kDexNoIndex = 0xFFFFFFFF;

template<typename T>
class DexIndex {
 public:
  T index_;

  constexpr DexIndex() : index_(std::numeric_limits<decltype(index_)>::max()) {}
  explicit constexpr DexIndex(T idx) : index_(idx) {}

  bool IsValid() const {
    return index_ != std::numeric_limits<decltype(index_)>::max();
  }
  static constexpr DexIndex Invalid() {
    return DexIndex(std::numeric_limits<decltype(index_)>::max());
  }
  bool operator==(const DexIndex& other) const {
    return index_ == other.index_;
  }
  bool operator!=(const DexIndex& other) const {
    return index_ != other.index_;
  }
  bool operator<(const DexIndex& other) const {
    return index_ < other.index_;
  }
  bool operator<=(const DexIndex& other) const {
    return index_ <= other.index_;
  }
  bool operator>(const DexIndex& other) const {
    return index_ > other.index_;
  }
  bool operator>=(const DexIndex& other) const {
    return index_ >= other.index_;
  }
};

class ProtoIndex : public DexIndex<uint16_t> {
 public:
  ProtoIndex() {}
  explicit constexpr ProtoIndex(uint16_t index) : DexIndex<decltype(index_)>(index) {}
  static constexpr ProtoIndex Invalid() {
    return ProtoIndex(std::numeric_limits<decltype(index_)>::max());
  }
};
std::ostream& operator<<(std::ostream& os, const ProtoIndex& index);

class StringIndex : public DexIndex<uint32_t> {
 public:
  StringIndex() {}
  explicit constexpr StringIndex(uint32_t index) : DexIndex<decltype(index_)>(index) {}
  static constexpr StringIndex Invalid() {
    return StringIndex(std::numeric_limits<decltype(index_)>::max());
  }
};
std::ostream& operator<<(std::ostream& os, const StringIndex& index);

class TypeIndex : public DexIndex<uint16_t> {
 public:
  TypeIndex() {}
  explicit constexpr TypeIndex(uint16_t index) : DexIndex<uint16_t>(index) {}
  static constexpr TypeIndex Invalid() {
    return TypeIndex(std::numeric_limits<decltype(index_)>::max());
  }
};
std::ostream& operator<<(std::ostream& os, const TypeIndex& index);

}  // namespace dex
}  // namespace art

namespace std {

template<> struct hash<art::dex::ProtoIndex> {
  size_t operator()(const art::dex::ProtoIndex& index) const {
    return hash<decltype(index.index_)>()(index.index_);
  }
};

template<> struct hash<art::dex::StringIndex> {
  size_t operator()(const art::dex::StringIndex& index) const {
    return hash<decltype(index.index_)>()(index.index_);
  }
};

template<> struct hash<art::dex::TypeIndex> {
  size_t operator()(const art::dex::TypeIndex& index) const {
    return hash<decltype(index.index_)>()(index.index_);
  }
};

}  // namespace std

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_TYPES_H_
