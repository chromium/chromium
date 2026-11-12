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

#ifndef ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_INL_H_
#define ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_INL_H_

#include "code_item_accessors.h"

#include "base/iteration_range.h"
#include "compact_dex_file.h"
#include "dex_file-inl.h"
#include "dex_instruction_iterator.h"
#include "standard_dex_file.h"

// The no ART version is used by binaries that don't include the whole runtime.
namespace art {

inline void CodeItemInstructionAccessor::Init(uint32_t insns_size_in_code_units,
                                              const uint16_t* insns) {
  insns_size_in_code_units_ = insns_size_in_code_units;
  insns_ = insns;
}

template <>
inline void CodeItemInstructionAccessor::Init<CompactDexFile::CodeItem>(
    const CompactDexFile::CodeItem& code_item) {
  uint32_t insns_size_in_code_units;
  code_item.DecodeFields</*kDecodeOnlyInstructionCount*/ true>(
      &insns_size_in_code_units,
      /*registers_size*/ nullptr,
      /*ins_size*/ nullptr,
      /*outs_size*/ nullptr,
      /*tries_size*/ nullptr);
  Init(insns_size_in_code_units, code_item.insns_);
}

template <>
inline void CodeItemInstructionAccessor::Init<StandardDexFile::CodeItem>(
    const StandardDexFile::CodeItem& code_item) {
  Init(code_item.insns_size_in_code_units_, code_item.insns_);
}

inline void CodeItemInstructionAccessor::Init(const DexFile& dex_file,
                                              const dex::CodeItem* code_item) {
  if (code_item != nullptr) {
    DCHECK(dex_file.IsInDataSection(code_item));
    if (dex_file.IsCompactDexFile()) {
      Init(down_cast<const CompactDexFile::CodeItem&>(*code_item));
    } else {
      DCHECK(dex_file.IsStandardDexFile());
      Init(down_cast<const StandardDexFile::CodeItem&>(*code_item));
    }
  }
}

inline CodeItemInstructionAccessor::CodeItemInstructionAccessor(
    const DexFile& dex_file,
    const dex::CodeItem* code_item) {
  Init(dex_file, code_item);
}

inline DexInstructionIterator CodeItemInstructionAccessor::begin() const {
  return DexInstructionIterator(insns_, 0u);
}

inline DexInstructionIterator CodeItemInstructionAccessor::end() const {
  return DexInstructionIterator(insns_, insns_size_in_code_units_);
}

inline IterationRange<DexInstructionIterator> CodeItemInstructionAccessor::InstructionsFrom(
    uint32_t start_dex_pc) const {
  DCHECK_LT(start_dex_pc, InsnsSizeInCodeUnits());
  return {
      DexInstructionIterator(insns_, start_dex_pc),
      DexInstructionIterator(insns_, insns_size_in_code_units_) };
}

template <>
inline void CodeItemDataAccessor::Init<CompactDexFile::CodeItem>(
    const CompactDexFile::CodeItem& code_item) {
  uint32_t insns_size_in_code_units;
  code_item.DecodeFields</*kDecodeOnlyInstructionCount*/ false>(&insns_size_in_code_units,
                                                                &registers_size_,
                                                                &ins_size_,
                                                                &outs_size_,
                                                                &tries_size_);
  CodeItemInstructionAccessor::Init(insns_size_in_code_units, code_item.insns_);
}

template <>
inline void CodeItemDataAccessor::Init<StandardDexFile::CodeItem>(
    const StandardDexFile::CodeItem& code_item) {
  CodeItemInstructionAccessor::Init(code_item);
  registers_size_ = code_item.registers_size_;
  ins_size_ = code_item.ins_size_;
  outs_size_ = code_item.outs_size_;
  tries_size_ = code_item.tries_size_;
}

inline void CodeItemDataAccessor::Init(const DexFile& dex_file,
                                       const dex::CodeItem* code_item) {
  if (code_item != nullptr) {
    if (dex_file.IsCompactDexFile()) {
      Init(down_cast<const CompactDexFile::CodeItem&>(*code_item));
    } else {
      DCHECK(dex_file.IsStandardDexFile());
      Init(down_cast<const StandardDexFile::CodeItem&>(*code_item));
    }
  }
}

inline CodeItemDataAccessor::CodeItemDataAccessor(const DexFile& dex_file,
                                                  const dex::CodeItem* code_item) {
  Init(dex_file, code_item);
}

inline IterationRange<const dex::TryItem*> CodeItemDataAccessor::TryItems() const {
  const dex::TryItem* try_items = DexFile::GetTryItems(end(), 0u);
  return {
    try_items,
    try_items + TriesSize() };
}

inline const uint8_t* CodeItemDataAccessor::GetCatchHandlerData(size_t offset) const {
  return DexFile::GetCatchHandlerData(end(), TriesSize(), offset);
}

inline const dex::TryItem* CodeItemDataAccessor::FindTryItem(uint32_t try_dex_pc) const {
  IterationRange<const dex::TryItem*> try_items(TryItems());
  int32_t index = DexFile::FindTryItem(try_items.begin(),
                                       try_items.end() - try_items.begin(),
                                       try_dex_pc);
  return index != -1 ? &try_items.begin()[index] : nullptr;
}

inline const void* CodeItemDataAccessor::CodeItemDataEnd() const {
  const uint8_t* handler_data = GetCatchHandlerData();

  if (TriesSize() == 0 || handler_data == nullptr) {
    return &end().Inst();
  }
  // Get the start of the handler data.
  const uint32_t handlers_size = DecodeUnsignedLeb128(&handler_data);
  // Manually read each handler.
  for (uint32_t i = 0; i < handlers_size; ++i) {
    int32_t uleb128_count = DecodeSignedLeb128(&handler_data) * 2;
    if (uleb128_count <= 0) {
      uleb128_count = -uleb128_count + 1;
    }
    for (int32_t j = 0; j < uleb128_count; ++j) {
      DecodeUnsignedLeb128(&handler_data);
    }
  }
  return reinterpret_cast<const void*>(handler_data);
}

template <>
inline void CodeItemDebugInfoAccessor::Init<CompactDexFile::CodeItem>(
    const CompactDexFile::CodeItem& code_item,
    uint32_t dex_method_index) {
  debug_info_offset_ = down_cast<const CompactDexFile*>(dex_file_)->GetDebugInfoOffset(
      dex_method_index);
  CodeItemDataAccessor::Init(code_item);
}

template <>
inline void CodeItemDebugInfoAccessor::Init<StandardDexFile::CodeItem>(
    const StandardDexFile::CodeItem& code_item, [[maybe_unused]] uint32_t dex_method_index) {
  debug_info_offset_ = code_item.debug_info_off_;
  CodeItemDataAccessor::Init(code_item);
}

inline void CodeItemDebugInfoAccessor::Init(const DexFile& dex_file,
                                            const dex::CodeItem* code_item,
                                            uint32_t dex_method_index) {
  dex_file_ = &dex_file;
  if (code_item == nullptr) {
    return;
  }
  if (dex_file.IsCompactDexFile()) {
    Init(down_cast<const CompactDexFile::CodeItem&>(*code_item), dex_method_index);
  } else {
    DCHECK(dex_file.IsStandardDexFile());
    Init(down_cast<const StandardDexFile::CodeItem&>(*code_item), dex_method_index);
  }
}

template<typename NewLocalVisitor>
inline bool CodeItemDebugInfoAccessor::DecodeDebugLocalInfo(
    bool is_static,
    uint32_t method_idx,
    const NewLocalVisitor& new_local) const {
  return dex_file_->DecodeDebugLocalInfo(RegistersSize(),
                                         InsSize(),
                                         InsnsSizeInCodeUnits(),
                                         DebugInfoOffset(),
                                         is_static,
                                         method_idx,
                                         new_local);
}

template <typename Visitor>
inline uint32_t CodeItemDebugInfoAccessor::VisitParameterNames(const Visitor& visitor) const {
  const uint8_t* stream = dex_file_->GetDebugInfoStream(DebugInfoOffset());
  return (stream != nullptr) ? DexFile::DecodeDebugInfoParameterNames(&stream, visitor) : 0u;
}

inline bool CodeItemDebugInfoAccessor::GetLineNumForPc(const uint32_t address,
                                                       uint32_t* line_num) const {
  return DecodeDebugPositionInfo([&](const DexFile::PositionInfo& entry) {
    // We know that this callback will be called in ascending address order, so keep going until we
    // find a match or we've just gone past it.
    if (entry.address_ > address) {
      // The line number from the previous positions callback will be the final result.
      return true;
    }
    *line_num = entry.line_;
    return entry.address_ == address;
  });
}

template <typename Visitor>
inline bool CodeItemDebugInfoAccessor::DecodeDebugPositionInfo(const Visitor& visitor) const {
  return dex_file_->DecodeDebugPositionInfo(
      dex_file_->GetDebugInfoStream(DebugInfoOffset()),
      [this](uint32_t idx) {
        return dex_file_->StringDataByIdx(dex::StringIndex(idx));
      },
      visitor);
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CODE_ITEM_ACCESSORS_INL_H_
