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

#include "type_lookup_table.h"

#include <cstring>
#include <memory>

#include "base/bit_utils.h"
#include "base/leb128.h"
#include "dex/dex_file-inl.h"
#include "dex/utf-inl.h"

namespace art {

static inline bool ModifiedUtf8StringEquals(const char* lhs, const char* rhs) {
  return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(lhs, rhs) == 0;
}

TypeLookupTable TypeLookupTable::Create(const DexFile& dex_file) {
  uint32_t num_class_defs = dex_file.NumClassDefs();
  if (UNLIKELY(!SupportedSize(num_class_defs))) {
    return TypeLookupTable();
  }
  size_t mask_bits = CalculateMaskBits(num_class_defs);
  size_t size = 1u << mask_bits;
  std::unique_ptr<Entry[]> owned_entries(new Entry[size]);
  Entry* entries = owned_entries.get();

  static_assert(alignof(Entry) == 4u, "Expecting Entry to be 4-byte aligned.");
  const uint32_t mask = Entry::GetMask(mask_bits);
  std::vector<uint16_t> conflict_class_defs;
  // The first stage. Put elements on their initial positions. If an initial position is already
  // occupied then delay the insertion of the element to the second stage to reduce probing
  // distance.
  for (size_t class_def_idx = 0; class_def_idx < dex_file.NumClassDefs(); ++class_def_idx) {
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_idx);
    const dex::TypeId& type_id = dex_file.GetTypeId(class_def.class_idx_);
    const dex::StringId& str_id = dex_file.GetStringId(type_id.descriptor_idx_);
    const uint32_t hash = ComputeModifiedUtf8Hash(dex_file.GetStringData(str_id));
    const uint32_t pos = hash & mask;
    if (entries[pos].IsEmpty()) {
      entries[pos] = Entry(str_id.string_data_off_, hash, class_def_idx, mask_bits);
      DCHECK(entries[pos].IsLast(mask_bits));
    } else {
      conflict_class_defs.push_back(class_def_idx);
    }
  }
  // The second stage. The initial position of these elements had a collision. Put these elements
  // into the nearest free cells and link them together by updating next_pos_delta.
  for (uint16_t class_def_idx : conflict_class_defs) {
    const dex::ClassDef& class_def = dex_file.GetClassDef(class_def_idx);
    const dex::TypeId& type_id = dex_file.GetTypeId(class_def.class_idx_);
    const dex::StringId& str_id = dex_file.GetStringId(type_id.descriptor_idx_);
    const uint32_t hash = ComputeModifiedUtf8Hash(dex_file.GetStringData(str_id));
    // Find the last entry in the chain.
    uint32_t tail_pos = hash & mask;
    DCHECK(!entries[tail_pos].IsEmpty());
    while (!entries[tail_pos].IsLast(mask_bits)) {
      tail_pos = (tail_pos + entries[tail_pos].GetNextPosDelta(mask_bits)) & mask;
      DCHECK(!entries[tail_pos].IsEmpty());
    }
    // Find an empty entry for insertion.
    uint32_t insert_pos = tail_pos;
    do {
      insert_pos = (insert_pos + 1) & mask;
    } while (!entries[insert_pos].IsEmpty());
    // Insert and chain the new entry.
    entries[insert_pos] = Entry(str_id.string_data_off_, hash, class_def_idx, mask_bits);
    entries[tail_pos].SetNextPosDelta((insert_pos - tail_pos) & mask, mask_bits);
    DCHECK(entries[insert_pos].IsLast(mask_bits));
    DCHECK(!entries[tail_pos].IsLast(mask_bits));
  }

  return TypeLookupTable(dex_file.DataBegin(), mask_bits, entries, std::move(owned_entries));
}

TypeLookupTable TypeLookupTable::Open(const uint8_t* dex_data_pointer,
                                      const uint8_t* raw_data,
                                      uint32_t num_class_defs) {
  DCHECK_ALIGNED(raw_data, alignof(Entry));
  const Entry* entries = reinterpret_cast<const Entry*>(raw_data);
  size_t mask_bits = CalculateMaskBits(num_class_defs);
  return TypeLookupTable(dex_data_pointer, mask_bits, entries, /* owned_entries= */ nullptr);
}

uint32_t TypeLookupTable::Lookup(const char* str, uint32_t hash) const {
  uint32_t mask = Entry::GetMask(mask_bits_);
  uint32_t pos = hash & mask;
  // Thanks to special insertion algorithm, the element at position pos can be empty
  // or start of the right bucket, or anywhere in the wrong bucket's chain.
  const Entry* entry = &entries_[pos];
  if (entry->IsEmpty()) {
    return dex::kDexNoIndex;
  }
  // Look for the partial hash match first, even if traversing the wrong bucket's chain.
  uint32_t compared_hash_bits = static_cast<uint64_t>(hash << mask_bits_) >> (2 * mask_bits_);
  while (compared_hash_bits != entry->GetHashBits(mask_bits_)) {
    if (entry->IsLast(mask_bits_)) {
      return dex::kDexNoIndex;
    }
    pos = (pos + entry->GetNextPosDelta(mask_bits_)) & mask;
    entry = &entries_[pos];
    DCHECK(!entry->IsEmpty());
  }
  // Found partial hash match, compare strings (expecting this to succeed).
  const char* first_checked_str = GetStringData(*entry);
  if (ModifiedUtf8StringEquals(str, first_checked_str)) {
    return entry->GetClassDefIdx(mask_bits_);
  }
  // If we're at the end of the chain, return before doing further expensive work.
  if (entry->IsLast(mask_bits_)) {
    return dex::kDexNoIndex;
  }
  // Check if we're traversing the right bucket. This is important if the compared
  // partial hash has only a few bits (i.e. it can match frequently).
  if (((ComputeModifiedUtf8Hash(first_checked_str) ^ hash) & mask) != 0u) {
    return dex::kDexNoIndex;  // Low hash bits mismatch.
  }
  // Continue looking for the string in the rest of the chain.
  do {
    pos = (pos + entry->GetNextPosDelta(mask_bits_)) & mask;
    entry = &entries_[pos];
    DCHECK(!entry->IsEmpty());
    if (compared_hash_bits == entry->GetHashBits(mask_bits_) &&
        ModifiedUtf8StringEquals(str, GetStringData(*entry))) {
      return entry->GetClassDefIdx(mask_bits_);
    }
  } while (!entry->IsLast(mask_bits_));
  // Not found.
  return dex::kDexNoIndex;
}

void TypeLookupTable::Dump(std::ostream& os) const {
  size_t size = 1u << mask_bits_;
  for (uint32_t i = 0; i < size; i++) {
    const Entry& entry = entries_[i];
    if (entry.IsEmpty()) {
      os << i << ": empty";
    } else {
      const char* first_checked_str = GetStringData(entry);
      os << i << ": " << std::string(first_checked_str);
    }
    os << '\n';
  }
}

uint32_t TypeLookupTable::RawDataLength(uint32_t num_class_defs) {
  return SupportedSize(num_class_defs) ? RoundUpToPowerOfTwo(num_class_defs) * sizeof(Entry) : 0u;
}

uint32_t TypeLookupTable::CalculateMaskBits(uint32_t num_class_defs) {
  return SupportedSize(num_class_defs) ? MinimumBitsToStore(num_class_defs - 1u) : 0u;
}

bool TypeLookupTable::SupportedSize(uint32_t num_class_defs) {
  return num_class_defs != 0u && num_class_defs <= std::numeric_limits<uint16_t>::max();
}

TypeLookupTable::TypeLookupTable(const uint8_t* dex_data_pointer,
                                 uint32_t mask_bits,
                                 const Entry* entries,
                                 std::unique_ptr<Entry[]> owned_entries)
    : dex_data_begin_(dex_data_pointer),
      mask_bits_(mask_bits),
      entries_(entries),
      owned_entries_(std::move(owned_entries)) {}

const char* TypeLookupTable::GetStringData(const Entry& entry) const {
  DCHECK(dex_data_begin_ != nullptr);
  const uint8_t* ptr = dex_data_begin_ + entry.GetStringOffset();
  // Skip string length.
  DecodeUnsignedLeb128(&ptr);
  return reinterpret_cast<const char*>(ptr);
}

}  // namespace art
