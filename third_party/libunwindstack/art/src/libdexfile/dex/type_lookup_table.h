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

#ifndef ART_LIBDEXFILE_DEX_TYPE_LOOKUP_TABLE_H_
#define ART_LIBDEXFILE_DEX_TYPE_LOOKUP_TABLE_H_

#include <android-base/logging.h>

#include "dex/dex_file_types.h"

namespace art {

class DexFile;

/**
 * TypeLookupTable used to find class_def_idx by class descriptor quickly.
 * Implementation of TypeLookupTable is based on hash table.
 * This class instantiated at compile time by calling Create() method and written into OAT file.
 * At runtime, the raw data is read from memory-mapped file by calling Open() method. The table
 * memory remains clean.
 */
class TypeLookupTable {
 public:
  // Method creates lookup table for dex file.
  static TypeLookupTable Create(const DexFile& dex_file);

  // Method opens lookup table from binary data. Lookups will traverse strings and other
  // data contained in dex_file as well.  Lookup table does not own raw_data or dex_file.
  static TypeLookupTable Open(const uint8_t* dex_data_pointer,
                              const uint8_t* raw_data,
                              uint32_t num_class_defs);

  // Create an invalid lookup table.
  TypeLookupTable()
      : dex_data_begin_(nullptr),
        mask_bits_(0u),
        entries_(nullptr),
        owned_entries_(nullptr) {}

  TypeLookupTable(TypeLookupTable&& src) noexcept = default;
  TypeLookupTable& operator=(TypeLookupTable&& src) noexcept = default;

  ~TypeLookupTable() {
    // Implicit deallocation by std::unique_ptr<> destructor.
  }

  // Returns whether the TypeLookupTable is valid.
  bool Valid() const {
    return entries_ != nullptr;
  }

  // Return the number of buckets in the lookup table.
  uint32_t Size() const {
    DCHECK(Valid());
    return 1u << mask_bits_;
  }

  // Method search class_def_idx by class descriptor and it's hash.
  // If no data found then the method returns dex::kDexNoIndex.
  uint32_t Lookup(const char* str, uint32_t hash) const;

  // Method returns pointer to binary data of lookup table. Used by the oat writer.
  const uint8_t* RawData() const {
    DCHECK(Valid());
    return reinterpret_cast<const uint8_t*>(entries_);
  }

  // Method returns length of binary data. Used by the oat writer.
  uint32_t RawDataLength() const {
    DCHECK(Valid());
    return Size() * sizeof(Entry);
  }

  // Method returns length of binary data for the specified number of class definitions.
  static uint32_t RawDataLength(uint32_t num_class_defs);

  void Dump(std::ostream& os) const;

 private:
  /**
   * To find element we need to compare strings.
   * It is faster to compare first hashes and then strings itself.
   * But we have no full hash of element of table. But we can use 2 ideas.
   * 1. All minor bits of hash inside one bucket are equal.
   *    (TODO: We're not actually using this, are we?)
   * 2. If the dex file contains N classes and the size of the hash table is 2^n (where N <= 2^n)
   *    then we need n bits for the class def index and n bits for the next position delta.
   *    So we can encode part of element's hash into the remaining 32-2*n (n <= 16) bits which
   *    would be otherwise wasted as a padding.
   * So hash of element can be divided on three parts:
   *     XXXX XXXX XXXY YYYY YYYY YZZZ ZZZZ ZZZZ  (example with n=11)
   *     Z - a part of hash encoded implicitly in the bucket index
   *         (these bits are same for all elements in bucket)
   *     Y - a part of hash that we can write into free 32-2*n bits
   *     X - a part of hash that we can't use without increasing the size of the entry
   * So the `data` element of Entry is used to store the next position delta, class_def_index
   * and a part of hash of the entry.
   */
  class Entry {
   public:
    Entry() : str_offset_(0u), data_(0u) {}
    Entry(uint32_t str_offset, uint32_t hash, uint32_t class_def_index, uint32_t mask_bits)
        : str_offset_(str_offset),
          data_(((hash & ~GetMask(mask_bits)) | class_def_index) << mask_bits) {
      DCHECK_EQ(class_def_index & ~GetMask(mask_bits), 0u);
    }

    void SetNextPosDelta(uint32_t next_pos_delta, uint32_t mask_bits) {
      DCHECK_EQ(GetNextPosDelta(mask_bits), 0u);
      DCHECK_EQ(next_pos_delta & ~GetMask(mask_bits), 0u);
      DCHECK_NE(next_pos_delta, 0u);
      data_ |= next_pos_delta;
    }

    bool IsEmpty() const {
      return str_offset_ == 0u;
    }

    bool IsLast(uint32_t mask_bits) const {
      return GetNextPosDelta(mask_bits) == 0u;
    }

    uint32_t GetStringOffset() const {
      return str_offset_;
    }

    uint32_t GetNextPosDelta(uint32_t mask_bits) const {
      return data_ & GetMask(mask_bits);
    }

    uint32_t GetClassDefIdx(uint32_t mask_bits) const {
      return (data_ >> mask_bits) & GetMask(mask_bits);
    }

    uint32_t GetHashBits(uint32_t mask_bits) const {
      DCHECK_LE(mask_bits, 16u);
      return static_cast<uint64_t>(data_) >> (2u * mask_bits);
    }

    static uint32_t GetMask(uint32_t mask_bits) {
      DCHECK_LE(mask_bits, 16u);
      return ~(std::numeric_limits<uint32_t>::max() << mask_bits);
    }

   private:
    uint32_t str_offset_;
    uint32_t data_;
  };

  static uint32_t CalculateMaskBits(uint32_t num_class_defs);
  static bool SupportedSize(uint32_t num_class_defs);

  // Construct the TypeLookupTable.
  TypeLookupTable(const uint8_t* dex_data_pointer,
                  uint32_t mask_bits,
                  const Entry* entries,
                  std::unique_ptr<Entry[]> owned_entries);

  const char* GetStringData(const Entry& entry) const;

  const uint8_t* dex_data_begin_;
  uint32_t mask_bits_;
  const Entry* entries_;
  // `owned_entries_` is either null (not owning `entries_`) or same pointer as `entries_`.
  std::unique_ptr<Entry[]> owned_entries_;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_TYPE_LOOKUP_TABLE_H_
