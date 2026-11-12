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

#include "compact_dex_file.h"

#include <algorithm>
#include <memory>

#include "base/leb128.h"
#include "code_item_accessors-inl.h"
#include "dex_file-inl.h"

namespace art {

void CompactDexFile::WriteMagic(uint8_t* magic) {
  std::copy_n(kDexMagic, kDexMagicSize, magic);
}

void CompactDexFile::WriteCurrentVersion(uint8_t* magic) {
  std::copy_n(kDexMagicVersion, kDexVersionLen, magic + kDexMagicSize);
}

bool CompactDexFile::IsMagicValid(const uint8_t* magic) {
  return (memcmp(magic, kDexMagic, sizeof(kDexMagic)) == 0);
}

bool CompactDexFile::IsVersionValid(const uint8_t* magic) {
  const uint8_t* version = &magic[sizeof(kDexMagic)];
  return memcmp(version, kDexMagicVersion, kDexVersionLen) == 0;
}

bool CompactDexFile::IsMagicValid() const {
  return IsMagicValid(header_->magic_);
}

bool CompactDexFile::IsVersionValid() const { return IsVersionValid(header_->magic_.data()); }

bool CompactDexFile::SupportsDefaultMethods() const {
  return (GetHeader().GetFeatureFlags() &
      static_cast<uint32_t>(FeatureFlags::kDefaultMethods)) != 0;
}

uint32_t CompactDexFile::GetCodeItemSize(const dex::CodeItem& item) const {
  DCHECK(IsInDataSection(&item));
  return reinterpret_cast<uintptr_t>(CodeItemDataAccessor(*this, &item).CodeItemDataEnd()) -
      reinterpret_cast<uintptr_t>(&item);
}


uint32_t CompactDexFile::CalculateChecksum(const uint8_t* base_begin,
                                           size_t base_size,
                                           const uint8_t* data_begin,
                                           size_t data_size) {
  Header temp_header(*Header::At(base_begin));
  // Zero out fields that are not included in the sum.
  temp_header.checksum_ = 0u;
  temp_header.data_off_ = 0u;
  temp_header.data_size_ = 0u;
  uint32_t checksum = ChecksumMemoryRange(reinterpret_cast<const uint8_t*>(&temp_header),
                                          sizeof(temp_header));
  // Exclude the header since we already computed it's checksum.
  checksum = (checksum * 31) ^ ChecksumMemoryRange(base_begin + sizeof(temp_header),
                                                   base_size - sizeof(temp_header));
  checksum = (checksum * 31) ^ ChecksumMemoryRange(data_begin, data_size);
  return checksum;
}

uint32_t CompactDexFile::CalculateChecksum() const {
  return CalculateChecksum(Begin(), Size(), DataBegin(), DataSize());
}

CompactDexFile::CompactDexFile(const uint8_t* base,
                               const std::string& location,
                               uint32_t location_checksum,
                               const OatDexFile* oat_dex_file,
                               std::shared_ptr<DexFileContainer> container)
    : DexFile(base,
              location,
              location_checksum,
              oat_dex_file,
              std::move(container),
              /*is_compact_dex=*/true),
      debug_info_offsets_(DataBegin() + GetHeader().debug_info_offsets_pos_,
                          GetHeader().debug_info_base_,
                          GetHeader().debug_info_offsets_table_offset_) {}

}  // namespace art
