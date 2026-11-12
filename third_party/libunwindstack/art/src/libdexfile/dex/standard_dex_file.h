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

#ifndef ART_LIBDEXFILE_DEX_STANDARD_DEX_FILE_H_
#define ART_LIBDEXFILE_DEX_STANDARD_DEX_FILE_H_

#include <iosfwd>
#include <memory>

#include "dex_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace art {

class OatDexFile;

// Standard dex file. This is the format that is packaged in APKs and produced by tools.
class StandardDexFile : public DexFile {
 public:
  class Header : public DexFile::Header {
    // Same for now.
  };

  struct CodeItem : public dex::CodeItem {
    static constexpr size_t kAlignment = 4;

    static constexpr size_t InsSizeOffset() {
      return OFFSETOF_MEMBER(CodeItem, ins_size_);
    }

    static constexpr size_t OutsSizeOffset() {
      return OFFSETOF_MEMBER(CodeItem, outs_size_);
    }

    static constexpr size_t RegistersSizeOffset() {
      return OFFSETOF_MEMBER(CodeItem, registers_size_);
    }

    static constexpr size_t InsnsOffset() {
      return OFFSETOF_MEMBER(CodeItem, insns_);
    }

   private:
    CodeItem() = default;

    uint16_t registers_size_;            // the number of registers used by this code
                                         //   (locals + parameters)
    uint16_t ins_size_;                  // the number of words of incoming arguments to the method
                                         //   that this code is for
    uint16_t outs_size_;                 // the number of words of outgoing argument space required
                                         //   by this code for method invocation
    uint16_t tries_size_;                // the number of try_items for this instance. If non-zero,
                                         //   then these appear as the tries array just after the
                                         //   insns in this instance.
    uint32_t debug_info_off_;            // Holds file offset to debug info stream.

    uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
    uint16_t insns_[1];                  // actual array of bytecode.

    ART_FRIEND_TEST(CodeItemAccessorsTest, TestDexInstructionsAccessor);
    friend class CodeItemDataAccessor;
    friend class CodeItemDebugInfoAccessor;
    friend class CodeItemInstructionAccessor;
    friend class DexWriter;
    friend class StandardDexFile;
    DISALLOW_COPY_AND_ASSIGN(CodeItem);
  };

  // Write the standard dex specific magic.
  static void WriteMagic(uint8_t* magic);

  // Write the current version, note that the input is the address of the magic.
  static void WriteCurrentVersion(uint8_t* magic);

  // Write the last version before default method support,
  // note that the input is the address of the magic.
  static void WriteVersionBeforeDefaultMethods(uint8_t* magic);

  static const uint8_t kDexMagic[kDexMagicSize];
  static constexpr size_t kNumDexVersions = 6;
  static const uint8_t kDexMagicVersions[kNumDexVersions][kDexVersionLen];

  // Returns true if the byte string points to the magic value.
  static bool IsMagicValid(const uint8_t* magic);
  static bool IsMagicValid(DexFile::Magic magic) { return IsMagicValid(magic.data()); }
  bool IsMagicValid() const override;

  // Returns true if the byte string after the magic is the correct value.
  static bool IsVersionValid(const uint8_t* magic);
  bool IsVersionValid() const override;

  bool SupportsDefaultMethods() const override;

  uint32_t GetCodeItemSize(const dex::CodeItem& item) const override;

  size_t GetDequickenedSize() const override {
    // JVMTI will run dex layout on standard dex files that have hidden API data,
    // in order to remove that data. As dexlayout may increase the size of the dex file,
    // be (very) conservative and add one MB to the size.
    return Size() + (HasHiddenapiClassData() ? 1 * MB : 0);
  }

 private:
  StandardDexFile(const uint8_t* base,
                  const std::string& location,
                  uint32_t location_checksum,
                  const OatDexFile* oat_dex_file,
                  // Shared since several dex files may be stored in the same logical container.
                  std::shared_ptr<DexFileContainer> container)
      : DexFile(base,
                location,
                location_checksum,
                oat_dex_file,
                std::move(container),
                /*is_compact_dex*/ false) {}

  friend class DexFileLoader;
  friend class DexFileVerifierTest;
  friend class FuzzerCorpusTest;  // for constructor

  ART_FRIEND_TEST(ClassLinkerTest, RegisterDexFileName);  // for constructor
  friend class OptimizingUnitTestHelper;  // for constructor
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);  // for constructor

  DISALLOW_COPY_AND_ASSIGN(StandardDexFile);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_STANDARD_DEX_FILE_H_
