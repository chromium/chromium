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

#ifndef ART_LIBDEXFILE_DEX_CLASS_ITERATOR_H_
#define ART_LIBDEXFILE_DEX_CLASS_ITERATOR_H_

#include <android-base/logging.h>

namespace art {

class ClassAccessor;
class ClassIterator;
class DexFile;

// Holder class, used to construct ClassAccessors.
class ClassIteratorData {
 public:
  ClassIteratorData(const DexFile& dex_file, uint32_t class_def_idx)
      : dex_file_(dex_file),
        class_def_idx_(class_def_idx) {}

 private:
  const DexFile& dex_file_;
  uint32_t class_def_idx_ = 0u;

  friend class ClassAccessor;
  friend class ClassIterator;
};

// Iterator for visiting classes in a Dex file.
class ClassIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = ClassIteratorData;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = void;

  ClassIterator(const DexFile& dex_file, uint32_t class_def_idx)
      : data_(dex_file, class_def_idx) {}

  // Value after modification.
  ClassIterator& operator++() {
    ++data_.class_def_idx_;
    return *this;
  }

  // Value before modification.
  ClassIterator operator++(int) {
    ClassIterator temp = *this;
    ++*this;
    return temp;
  }

  const value_type& operator*() const {
    return data_;
  }

  bool operator==(const ClassIterator& rhs) const {
    DCHECK_EQ(&data_.dex_file_, &rhs.data_.dex_file_) << "Comparing different dex files.";
    return data_.class_def_idx_ == rhs.data_.class_def_idx_;
  }

  bool operator!=(const ClassIterator& rhs) const {
    return !(*this == rhs);
  }

  bool operator<(const ClassIterator& rhs) const {
    DCHECK_EQ(&data_.dex_file_, &rhs.data_.dex_file_) << "Comparing different dex files.";
    return data_.class_def_idx_ < rhs.data_.class_def_idx_;
  }

  bool operator>(const ClassIterator& rhs) const {
    return rhs < *this;
  }

  bool operator<=(const ClassIterator& rhs) const {
    return !(rhs < *this);
  }

  bool operator>=(const ClassIterator& rhs) const {
    return !(*this < rhs);
  }

 protected:
  ClassIteratorData data_;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CLASS_ITERATOR_H_
