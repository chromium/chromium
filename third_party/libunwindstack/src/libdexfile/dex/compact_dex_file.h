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

#ifndef ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_
#define ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_

#include <memory>

#include "base/casts.h"
#include "dex/compact_offset_table.h"
#include "dex_file.h"

namespace art {

// CompactDex is a currently ART internal dex file format that aims to reduce storage/RAM usage.
class CompactDexFile : public DexFile {
 public:
  static constexpr uint8_t kDexMagic[kDexMagicSize] = { 'c', 'd', 'e', 'x' };
  // Last change: remove code item deduping.
  static constexpr uint8_t kDexMagicVersion[] = {'0', '0', '1', '\0'};

  enum class FeatureFlags : uint32_t {
    kDefaultMethods = 0x1,
  };

  class Header : public DexFile::Header {
   public:
    static const Header* At(const void* at) {
      return reinterpret_cast<const Header*>(at);
    }

    uint32_t GetFeatureFlags() const {
      return feature_flags_;
    }

    uint32_t GetDataOffset() const {
      return data_off_;
    }

    uint32_t GetDataSize() const {
      return data_size_;
    }

    // Range of the shared data section owned by the dex file. Owned in this context refers to data
    // for this DEX that was not deduplicated to another DEX.
    uint32_t OwnedDataBegin() const {
      return owned_data_begin_;
    }

    uint32_t OwnedDataEnd() const {
      return owned_data_end_;
    }

   private:
    uint32_t feature_flags_ = 0u;

    // Position in the compact dex file for the debug info table data starts.
    uint32_t debug_info_offsets_pos_ = 0u;

    // Offset into the debug info table data where the lookup table is.
    uint32_t debug_info_offsets_table_offset_ = 0u;

    // Base offset of where debug info starts in the dex file.
    uint32_t debug_info_base_ = 0u;

    // Range of the shared data section owned by the dex file.
    uint32_t owned_data_begin_ = 0u;
    uint32_t owned_data_end_ = 0u;

    friend class CompactDexFile;
    friend class CompactDexWriter;
  };

  // Like the standard code item except without a debug info offset. Each code item may have a
  // preheader to encode large methods. In 99% of cases, the preheader is not used. This enables
  // smaller size with a good fast path case in the accessors.
  struct CodeItem : public dex::CodeItem {
    static constexpr size_t kAlignment = sizeof(uint16_t);
    // Max preheader size in uint16_ts.
    static constexpr size_t kMaxPreHeaderSize = 6;

    static constexpr size_t FieldsOffset() {
      return OFFSETOF_MEMBER(CodeItem, fields_);
    }

    static constexpr size_t InsnsCountAndFlagsOffset() {
      return OFFSETOF_MEMBER(CodeItem, insns_count_and_flags_);
    }

    static constexpr size_t InsnsOffset() {
      return OFFSETOF_MEMBER(CodeItem, insns_);
    }

    static constexpr size_t kRegistersSizeShift = 12;
    static constexpr size_t kInsSizeShift = 8;
    static constexpr size_t kOutsSizeShift = 4;
    static constexpr size_t kTriesSizeSizeShift = 0;
    static constexpr uint16_t kBitPreHeaderRegistersSize = 0;
    static constexpr uint16_t kBitPreHeaderInsSize = 1;
    static constexpr uint16_t kBitPreHeaderOutsSize = 2;
    static constexpr uint16_t kBitPreHeaderTriesSize = 3;
    static constexpr uint16_t kBitPreHeaderInsnsSize = 4;
    static constexpr uint16_t kFlagPreHeaderRegistersSize = 0x1 << kBitPreHeaderRegistersSize;
    static constexpr uint16_t kFlagPreHeaderInsSize = 0x1 << kBitPreHeaderInsSize;
    static constexpr uint16_t kFlagPreHeaderOutsSize = 0x1 << kBitPreHeaderOutsSize;
    static constexpr uint16_t kFlagPreHeaderTriesSize = 0x1 << kBitPreHeaderTriesSize;
    static constexpr uint16_t kFlagPreHeaderInsnsSize = 0x1 << kBitPreHeaderInsnsSize;
    static constexpr size_t kInsnsSizeShift = 5;
    static constexpr size_t kInsnsSizeBits = sizeof(uint16_t) * kBitsPerByte -  kInsnsSizeShift;

   private:
    CodeItem() = default;

    // Combined preheader flags for fast testing if we need to go slow path.
    static constexpr uint16_t kFlagPreHeaderCombined =
        kFlagPreHeaderRegistersSize |
        kFlagPreHeaderInsSize |
        kFlagPreHeaderOutsSize |
        kFlagPreHeaderTriesSize |
        kFlagPreHeaderInsnsSize;

    // Create a code item and associated preheader if required based on field values.
    // Returns the start of the preheader. The preheader buffer must be at least as large as
    // kMaxPreHeaderSize;
    uint16_t* Create(uint16_t registers_size,
                     uint16_t ins_size,
                     uint16_t outs_size,
                     uint16_t tries_size,
                     uint32_t insns_size_in_code_units,
                     uint16_t* out_preheader) {
      // Dex verification ensures that registers size > ins_size, so we can subtract the registers
      // size accordingly to reduce how often we need to use the preheader.
      DCHECK_GE(registers_size, ins_size);
      registers_size -= ins_size;
      fields_ = (registers_size & 0xF) << kRegistersSizeShift;
      fields_ |= (ins_size & 0xF) << kInsSizeShift;
      fields_ |= (outs_size & 0xF) << kOutsSizeShift;
      fields_ |= (tries_size & 0xF) << kTriesSizeSizeShift;
      registers_size &= ~0xF;
      ins_size &= ~0xF;
      outs_size &= ~0xF;
      tries_size &= ~0xF;
      insns_count_and_flags_ = 0;
      const size_t masked_count = insns_size_in_code_units & ((1 << kInsnsSizeBits) - 1);
      insns_count_and_flags_ |= masked_count << kInsnsSizeShift;
      insns_size_in_code_units -= masked_count;

      // Since the preheader case is rare (1% of code items), use a suboptimally large but fast
      // decoding format.
      if (insns_size_in_code_units != 0) {
        insns_count_and_flags_ |= kFlagPreHeaderInsnsSize;
        --out_preheader;
        *out_preheader = static_cast<uint16_t>(insns_size_in_code_units);
        --out_preheader;
        *out_preheader = static_cast<uint16_t>(insns_size_in_code_units >> 16);
      }
      auto preheader_encode = [&](uint16_t size, uint16_t flag) {
        if (size != 0) {
          insns_count_and_flags_ |= flag;
          --out_preheader;
          *out_preheader = size;
        }
      };
      preheader_encode(registers_size, kFlagPreHeaderRegistersSize);
      preheader_encode(ins_size, kFlagPreHeaderInsSize);
      preheader_encode(outs_size, kFlagPreHeaderOutsSize);
      preheader_encode(tries_size, kFlagPreHeaderTriesSize);
      return out_preheader;
    }

    ALWAYS_INLINE bool HasPreHeader(uint16_t flag) const {
      return (insns_count_and_flags_ & flag) != 0;
    }

    // Return true if the code item has any preheaders.
    ALWAYS_INLINE static bool HasAnyPreHeader(uint16_t insns_count_and_flags) {
      return (insns_count_and_flags & kFlagPreHeaderCombined) != 0;
    }

    ALWAYS_INLINE uint16_t* GetPreHeader() {
      return reinterpret_cast<uint16_t*>(this);
    }

    ALWAYS_INLINE const uint16_t* GetPreHeader() const {
      return reinterpret_cast<const uint16_t*>(this);
    }

    // Decode fields and read the preheader if necessary. If kDecodeOnlyInstructionCount is
    // specified then only the instruction count is decoded.
    template <bool kDecodeOnlyInstructionCount>
    ALWAYS_INLINE void DecodeFields(uint32_t* insns_count,
                                    uint16_t* registers_size,
                                    uint16_t* ins_size,
                                    uint16_t* outs_size,
                                    uint16_t* tries_size) const {
      *insns_count = insns_count_and_flags_ >> kInsnsSizeShift;
      if (!kDecodeOnlyInstructionCount) {
        const uint16_t fields = fields_;
        *registers_size = (fields >> kRegistersSizeShift) & 0xF;
        *ins_size = (fields >> kInsSizeShift) & 0xF;
        *outs_size = (fields >> kOutsSizeShift) & 0xF;
        *tries_size = (fields >> kTriesSizeSizeShift) & 0xF;
      }
      if (UNLIKELY(HasAnyPreHeader(insns_count_and_flags_))) {
        const uint16_t* preheader = GetPreHeader();
        if (HasPreHeader(kFlagPreHeaderInsnsSize)) {
          --preheader;
          *insns_count += static_cast<uint32_t>(*preheader);
          --preheader;
          *insns_count += static_cast<uint32_t>(*preheader) << 16;
        }
        if (!kDecodeOnlyInstructionCount) {
          if (HasPreHeader(kFlagPreHeaderRegistersSize)) {
            --preheader;
            *registers_size += preheader[0];
          }
          if (HasPreHeader(kFlagPreHeaderInsSize)) {
            --preheader;
            *ins_size += preheader[0];
          }
          if (HasPreHeader(kFlagPreHeaderOutsSize)) {
            --preheader;
            *outs_size += preheader[0];
          }
          if (HasPreHeader(kFlagPreHeaderTriesSize)) {
            --preheader;
            *tries_size += preheader[0];
          }
        }
      }
      if (!kDecodeOnlyInstructionCount) {
        *registers_size += *ins_size;
      }
    }

    // Packed code item data, 4 bits each: [registers_size, ins_size, outs_size, tries_size]
    uint16_t fields_;

    // 5 bits for if either of the fields required preheader extension, 11 bits for the number of
    // instruction code units.
    uint16_t insns_count_and_flags_;

    uint16_t insns_[1];                  // actual array of bytecode.

    ART_FRIEND_TEST(CodeItemAccessorsTest, TestDexInstructionsAccessor);
    ART_FRIEND_TEST(CompactDexFileTest, CodeItemFields);
    friend class CodeItemDataAccessor;
    friend class CodeItemDebugInfoAccessor;
    friend class CodeItemInstructionAccessor;
    friend class CompactDexFile;
    friend class CompactDexWriter;
    DISALLOW_COPY_AND_ASSIGN(CodeItem);
  };

  // Write the compact dex specific magic.
  static void WriteMagic(uint8_t* magic);

  // Write the current version, note that the input is the address of the magic.
  static void WriteCurrentVersion(uint8_t* magic);

  // Returns true if the byte string points to the magic value.
  static bool IsMagicValid(const uint8_t* magic);
  static bool IsMagicValid(DexFile::Magic magic) { return IsMagicValid(magic.data()); }
  bool IsMagicValid() const override;

  // Returns true if the byte string after the magic is the correct value.
  static bool IsVersionValid(const uint8_t* magic);
  bool IsVersionValid() const override;

  // TODO This is completely a guess. We really need to do better. b/72402467
  // We ask for 64 megabytes which should be big enough for any realistic dex file.
  size_t GetDequickenedSize() const override {
    return 64 * MB;
  }

  const Header& GetHeader() const {
    return down_cast<const Header&>(DexFile::GetHeader());
  }

  bool SupportsDefaultMethods() const override;

  uint32_t GetCodeItemSize(const dex::CodeItem& item) const override;

  uint32_t GetDebugInfoOffset(uint32_t dex_method_index) const {
    return debug_info_offsets_.GetOffset(dex_method_index);
  }

  static uint32_t CalculateChecksum(const uint8_t* base_begin,
                                    size_t base_size,
                                    const uint8_t* data_begin,
                                    size_t data_size);
  uint32_t CalculateChecksum() const override;

 private:
  CompactDexFile(const uint8_t* base,
                 const std::string& location,
                 uint32_t location_checksum,
                 const OatDexFile* oat_dex_file,
                 // Shared since several dex files may be stored in the same logical container.
                 std::shared_ptr<DexFileContainer> container);

  CompactOffsetTable::Accessor debug_info_offsets_;

  friend class DexFile;
  friend class DexFileLoader;
  DISALLOW_COPY_AND_ASSIGN(CompactDexFile);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_COMPACT_DEX_FILE_H_
