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

// TODO: Dex helpers have ART specific APIs, we may want to refactor these for use in dexdump.

#ifndef ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_H_
#define ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_H_

#include <android-base/logging.h>

#include "dex_instruction.h"

namespace art {

namespace dex {
struct CodeItem;
struct TryItem;
}  // namespace dex

class ArtMethod;
class DexFile;
class DexInstructionIterator;
template <typename Iter>
class IterationRange;

// Abstracts accesses to the instruction fields of code items for CompactDexFile and
// StandardDexFile.
class CodeItemInstructionAccessor {
 public:
  ALWAYS_INLINE CodeItemInstructionAccessor(const DexFile& dex_file,
                                            const dex::CodeItem* code_item);

  ALWAYS_INLINE explicit CodeItemInstructionAccessor(ArtMethod* method);

  ALWAYS_INLINE DexInstructionIterator begin() const;

  ALWAYS_INLINE DexInstructionIterator end() const;

  IterationRange<DexInstructionIterator> InstructionsFrom(uint32_t start_dex_pc) const;

  uint32_t InsnsSizeInCodeUnits() const {
    return insns_size_in_code_units_;
  }

  uint32_t InsnsSizeInBytes() const {
    static constexpr uint32_t kCodeUnitSizeInBytes = 2u;
    return insns_size_in_code_units_ * kCodeUnitSizeInBytes;
  }

  const uint16_t* Insns() const {
    return insns_;
  }

  // Return the instruction for a dex pc.
  const Instruction& InstructionAt(uint32_t dex_pc) const {
    DCHECK_LT(dex_pc, InsnsSizeInCodeUnits());
    return *Instruction::At(insns_ + dex_pc);
  }

  // Return true if the accessor has a code item.
  bool HasCodeItem() const {
    return Insns() != nullptr;
  }

 protected:
  CodeItemInstructionAccessor() = default;

  ALWAYS_INLINE void Init(uint32_t insns_size_in_code_units, const uint16_t* insns);
  ALWAYS_INLINE void Init(const DexFile& dex_file, const dex::CodeItem* code_item);

  template <typename DexFileCodeItemType>
  ALWAYS_INLINE void Init(const DexFileCodeItemType& code_item);

 private:
  // size of the insns array, in 2 byte code units. 0 if there is no code item.
  uint32_t insns_size_in_code_units_ = 0;

  // Pointer to the instructions, null if there is no code item.
  const uint16_t* insns_ = nullptr;
};

// Abstracts accesses to code item fields other than debug info for CompactDexFile and
// StandardDexFile.
class CodeItemDataAccessor : public CodeItemInstructionAccessor {
 public:
  ALWAYS_INLINE CodeItemDataAccessor(const DexFile& dex_file, const dex::CodeItem* code_item);

  uint16_t RegistersSize() const {
    return registers_size_;
  }

  uint16_t InsSize() const {
    return ins_size_;
  }

  uint16_t OutsSize() const {
    return outs_size_;
  }

  uint16_t TriesSize() const {
    return tries_size_;
  }

  IterationRange<const dex::TryItem*> TryItems() const;

  const uint8_t* GetCatchHandlerData(size_t offset = 0) const;

  const dex::TryItem* FindTryItem(uint32_t try_dex_pc) const;

  inline const void* CodeItemDataEnd() const;

 protected:
  CodeItemDataAccessor() = default;

  ALWAYS_INLINE void Init(const DexFile& dex_file, const dex::CodeItem* code_item);

  template <typename DexFileCodeItemType>
  ALWAYS_INLINE void Init(const DexFileCodeItemType& code_item);

 private:
  // Fields mirrored from the dex/cdex code item.
  uint16_t registers_size_;
  uint16_t ins_size_;
  uint16_t outs_size_;
  uint16_t tries_size_;
};

// Abstract accesses to code item data including debug info offset. More heavy weight than the other
// helpers.
class CodeItemDebugInfoAccessor : public CodeItemDataAccessor {
 public:
  CodeItemDebugInfoAccessor() = default;

  // Initialize with an existing offset.
  ALWAYS_INLINE CodeItemDebugInfoAccessor(const DexFile& dex_file,
                                          const dex::CodeItem* code_item,
                                          uint32_t dex_method_index) {
    Init(dex_file, code_item, dex_method_index);
  }

  ALWAYS_INLINE void Init(const DexFile& dex_file,
                          const dex::CodeItem* code_item,
                          uint32_t dex_method_index);

  ALWAYS_INLINE explicit CodeItemDebugInfoAccessor(ArtMethod* method);

  uint32_t DebugInfoOffset() const {
    return debug_info_offset_;
  }

  template<typename NewLocalVisitor>
  bool DecodeDebugLocalInfo(bool is_static,
                            uint32_t method_idx,
                            const NewLocalVisitor& new_local) const;

  // Visit each parameter in the debug information. Returns the line number.
  // The argument of the Visitor is dex::StringIndex.
  template <typename Visitor>
  uint32_t VisitParameterNames(const Visitor& visitor) const;

  template <typename Visitor>
  bool DecodeDebugPositionInfo(const Visitor& visitor) const;

  bool GetLineNumForPc(const uint32_t pc, uint32_t* line_num) const;

 protected:
  template <typename DexFileCodeItemType>
  ALWAYS_INLINE void Init(const DexFileCodeItemType& code_item, uint32_t dex_method_index);

 private:
  const DexFile* dex_file_ = nullptr;
  uint32_t debug_info_offset_ = 0u;
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_H_
