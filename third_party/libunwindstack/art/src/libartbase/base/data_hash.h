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

#ifndef ART_LIBARTBASE_BASE_DATA_HASH_H_
#define ART_LIBARTBASE_BASE_DATA_HASH_H_

#include <type_traits>

#include "base/globals.h"
#include "base/macros.h"

namespace art {

// Note: Touching this file or any #included file causes too many files to be rebuilt, so
// we want to avoid #including any files that are not necessary. Therefore we use templates
// (and std::enable_if<>) to avoid `#including headers for `ArrayRef<>` or `BitMemoryRegion`.
class BitMemoryRegion;

class DataHash {
 private:
  static constexpr bool kUseMurmur3Hash = true;

 public:
  template <class Container,
            typename = std::enable_if_t<!std::is_same_v<Container, BitMemoryRegion>>>
  size_t operator()(const Container& array) const {
    // Containers that provide the data() function use contiguous storage.
    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
    uint32_t length_in_bytes = sizeof(typename Container::value_type) * array.size();
    if (kUseMurmur3Hash) {
      uint32_t hash = Murmur3Start();

      const size_t nblocks = length_in_bytes / 4;
      using unaligned_uint32_t __attribute__((__aligned__(1))) = uint32_t;
      const unaligned_uint32_t* blocks = reinterpret_cast<const unaligned_uint32_t*>(data);
      for (size_t i = 0; i != nblocks; ++i) {
        hash = Murmur3Update(hash, blocks[i]);
      }

      const uint8_t* tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);
      uint32_t last_block = 0;

      switch (length_in_bytes & 3) {
        case 3:
          last_block ^= tail[2] << 16;
          FALLTHROUGH_INTENDED;
        case 2:
          last_block ^= tail[1] << 8;
          FALLTHROUGH_INTENDED;
        case 1:
          last_block ^= tail[0];
          hash = Murmur3UpdatePartial(hash, last_block);
      }

      hash = Murmur3Finish(hash, length_in_bytes);
      return hash;
    } else {
      return HashBytes(data, length_in_bytes);
    }
  }

  // Hash bytes using a relatively fast hash.
  static inline size_t HashBytes(const uint8_t* data, size_t length_in_bytes) {
    size_t hash = HashBytesStart();
    for (uint32_t i = 0; i != length_in_bytes; ++i) {
      hash = HashBytesUpdate(hash, data[i]);
    }
    return HashBytesFinish(hash);
  }

  template <typename BMR,
            typename = std::enable_if_t<std::is_same_v<BMR, BitMemoryRegion>>>
  size_t operator()(BMR region) const {
    if (kUseMurmur3Hash) {
      size_t num_full_blocks = region.size_in_bits() / kMurmur3BlockBits;
      size_t num_end_bits = region.size_in_bits() % kMurmur3BlockBits;
      size_t hash = Murmur3Start();
      for (uint32_t i = 0; i != num_full_blocks; ++i) {
        uint32_t block = region.LoadBits(i * kMurmur3BlockBits, kMurmur3BlockBits);
        hash = Murmur3Update(hash, block);
      }
      if (num_end_bits != 0u) {
        uint32_t end_bits = region.LoadBits(num_full_blocks * kMurmur3BlockBits, num_end_bits);
        hash = Murmur3UpdatePartial(hash, end_bits);
      }
      return HashBytesFinish(hash);
    } else {
      size_t num_full_bytes = region.size_in_bits() / kBitsPerByte;
      size_t num_end_bits = region.size_in_bits() % kBitsPerByte;
      size_t hash = HashBytesStart();
      for (uint32_t i = 0; i != num_full_bytes; ++i) {
        uint8_t byte = region.LoadBits(i * kBitsPerByte, kBitsPerByte);
        hash = HashBytesUpdate(hash, byte);
      }
      if (num_end_bits != 0u) {
        uint32_t end_bits = region.LoadBits(num_full_bytes * kBitsPerByte, num_end_bits);
        hash = HashBytesUpdate(hash, end_bits);
      }
      return HashBytesFinish(hash);
    }
  }

 private:
  ALWAYS_INLINE
  static constexpr size_t HashBytesStart() {
    return 0x811c9dc5;
  }

  ALWAYS_INLINE
  static constexpr size_t HashBytesUpdate(size_t hash, uint8_t value) {
    return (hash * 16777619) ^ value;
  }

  ALWAYS_INLINE
  static constexpr size_t HashBytesFinish(size_t hash) {
    hash += hash << 13;
    hash ^= hash >> 7;
    hash += hash << 3;
    hash ^= hash >> 17;
    hash += hash << 5;
    return hash;
  }

  static constexpr uint32_t kMurmur3Seed = 0u;
  static constexpr uint32_t kMurmur3BlockBits = 32u;
  static constexpr uint32_t kMurmur3C1 = 0xcc9e2d51;
  static constexpr uint32_t kMurmur3C2 = 0x1b873593;
  static constexpr uint32_t kMurmur3R1 = 15;
  static constexpr uint32_t kMurmur3R2 = 13;
  static constexpr uint32_t kMurmur3M = 5;
  static constexpr uint32_t kMurmur3N = 0xe6546b64;

  ALWAYS_INLINE
  static constexpr uint32_t Murmur3Start() {
    return kMurmur3Seed;
  }

  ALWAYS_INLINE
  static constexpr uint32_t Murmur3Update(uint32_t hash, uint32_t block) {
    uint32_t k = block;
    k *= kMurmur3C1;
    k = (k << kMurmur3R1) | (k >> (32 - kMurmur3R1));
    k *= kMurmur3C2;
    hash ^= k;
    hash = ((hash << kMurmur3R2) | (hash >> (32 - kMurmur3R2))) * kMurmur3M + kMurmur3N;
    return hash;
  }

  ALWAYS_INLINE
  static constexpr uint32_t Murmur3UpdatePartial(uint32_t hash, uint32_t block) {
    uint32_t k = block;
    k *= kMurmur3C1;
    k = (k << kMurmur3R1) | (k >> (32 - kMurmur3R1));
    k *= kMurmur3C2;
    hash ^= k;
    // Note: Unlike full block, the partial block does not have `hash = hash * M + N`.
    return hash;
  }

  ALWAYS_INLINE
  static constexpr uint32_t Murmur3Finish(uint32_t hash, uint32_t length_in_bytes) {
    hash ^= length_in_bytes;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);
    return hash;
  }
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_DATA_HASH_H_
