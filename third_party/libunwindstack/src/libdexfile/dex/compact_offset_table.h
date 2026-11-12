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

#ifndef ART_LIBDEXFILE_DEX_COMPACT_OFFSET_TABLE_H_
#define ART_LIBDEXFILE_DEX_COMPACT_OFFSET_TABLE_H_

#include <cstdint>
#include <vector>

namespace art {

// Compact offset table that aims to minimize size while still providing reasonable speed (10-20ns
// average time per lookup on host).
class CompactOffsetTable {
 public:
  // This value is coupled with the leb chunk bitmask. That logic must also be adjusted when the
  // integer is modified.
  static constexpr size_t kElementsPerIndex = 16;

  // Leb block format:
  // [uint16_t] 16 bit mask for what indexes actually have a non zero offset for the chunk.
  // [lebs] Up to 16 lebs encoded using leb128, one leb bit. The leb specifies how the offset
  // changes compared to the previous index.

  class Accessor {
   public:
    // Read the minimum and table offsets from the data pointer.
    explicit Accessor(const uint8_t* data_begin);

    Accessor(const uint8_t* data_begin, uint32_t minimum_offset, uint32_t table_offset);

    // Return the offset for the index.
    uint32_t GetOffset(uint32_t index) const;

   private:
    const uint32_t* const table_;
    const uint32_t minimum_offset_;
    const uint8_t* const data_begin_;
  };

  // Version that also serializes the min offset and table offset.
  static void Build(const std::vector<uint32_t>& offsets, std::vector<uint8_t>* out_data);

  // Returned offsets are all relative to out_min_offset.
  static void Build(const std::vector<uint32_t>& offsets,
                    std::vector<uint8_t>* out_data,
                    uint32_t* out_min_offset,
                    uint32_t* out_table_offset);

  // 32 bit aligned for the offset table.
  static constexpr size_t kAlignment = sizeof(uint32_t);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_COMPACT_OFFSET_TABLE_H_
