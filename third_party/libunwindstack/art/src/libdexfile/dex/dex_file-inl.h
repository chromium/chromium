/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_INL_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_INL_H_

#include "dex_file.h"

#include "base/casts.h"
#include "base/iteration_range.h"
#include "base/leb128.h"
#include "base/utils.h"
#include "class_iterator.h"
#include "compact_dex_file.h"
#include "dex_instruction_iterator.h"
#include "invoke_type.h"
#include "signature.h"
#include "standard_dex_file.h"

namespace art {

inline std::string_view StringViewFromUtf16Length(const char* utf8_data, size_t utf16_length) {
  size_t utf8_length = LIKELY(utf8_data[utf16_length] == 0)  // Is ASCII?
                           ? utf16_length
                           : utf16_length + strlen(utf8_data + utf16_length);
  return std::string_view(utf8_data, utf8_length);
}

inline int32_t DexFile::GetStringLength(const dex::StringId& string_id) const {
  const uint8_t* ptr = DataBegin() + string_id.string_data_off_;
  return DecodeUnsignedLeb128(&ptr);
}

ALWAYS_INLINE
inline const char* DexFile::GetStringDataAndUtf16Length(const dex::StringId& string_id,
                                                        uint32_t* utf16_length) const {
  DCHECK(utf16_length != nullptr) << GetLocation();
  const uint8_t* ptr = DataBegin() + string_id.string_data_off_;
  *utf16_length = DecodeUnsignedLeb128(&ptr);
  return reinterpret_cast<const char*>(ptr);
}

ALWAYS_INLINE
inline const char* DexFile::GetStringData(const dex::StringId& string_id) const {
  uint32_t ignored;
  return GetStringDataAndUtf16Length(string_id, &ignored);
}

ALWAYS_INLINE
inline const char* DexFile::StringDataAndUtf16LengthByIdx(dex::StringIndex idx,
                                                          uint32_t* utf16_length) const {
  if (!idx.IsValid()) {
    *utf16_length = 0;
    return nullptr;
  }
  const dex::StringId& string_id = GetStringId(idx);
  return GetStringDataAndUtf16Length(string_id, utf16_length);
}

ALWAYS_INLINE
inline const char* DexFile::StringDataByIdx(dex::StringIndex idx) const {
  uint32_t unicode_length;
  return StringDataAndUtf16LengthByIdx(idx, &unicode_length);
}

ALWAYS_INLINE
inline std::string_view DexFile::StringViewByIdx(dex::StringIndex idx) const {
  uint32_t unicode_length;
  const char* data = StringDataAndUtf16LengthByIdx(idx, &unicode_length);
  return data != nullptr ? StringViewFromUtf16Length(data, unicode_length) : std::string_view("");
}

inline const char* DexFile::StringByTypeIdx(dex::TypeIndex idx, uint32_t* unicode_length) const {
  if (!idx.IsValid()) {
    return nullptr;
  }
  const dex::TypeId& type_id = GetTypeId(idx);
  return StringDataAndUtf16LengthByIdx(type_id.descriptor_idx_, unicode_length);
}

inline const char* DexFile::StringByTypeIdx(dex::TypeIndex idx) const {
  if (!idx.IsValid()) {
    return nullptr;
  }
  const dex::TypeId& type_id = GetTypeId(idx);
  return StringDataByIdx(type_id.descriptor_idx_);
}

inline const char* DexFile::GetTypeDescriptor(const dex::TypeId& type_id) const {
  return StringDataByIdx(type_id.descriptor_idx_);
}

inline std::string_view DexFile::GetTypeDescriptorView(const dex::TypeId& type_id) const {
  return StringViewByIdx(type_id.descriptor_idx_);
}

inline const char* DexFile::GetFieldTypeDescriptor(const dex::FieldId& field_id) const {
  const dex::TypeId& type_id = GetTypeId(field_id.type_idx_);
  return GetTypeDescriptor(type_id);
}

inline std::string_view DexFile::GetFieldTypeDescriptorView(const dex::FieldId& field_id) const {
  const dex::TypeId& type_id = GetTypeId(field_id.type_idx_);
  return GetTypeDescriptorView(type_id);
}

inline const char* DexFile::GetFieldName(const dex::FieldId& field_id) const {
  return StringDataByIdx(field_id.name_idx_);
}

inline std::string_view DexFile::GetFieldNameView(const dex::FieldId& field_id) const {
  return StringViewByIdx(field_id.name_idx_);
}

inline const char* DexFile::GetMethodDeclaringClassDescriptor(const dex::MethodId& method_id)
    const {
  const dex::TypeId& type_id = GetTypeId(method_id.class_idx_);
  return GetTypeDescriptor(type_id);
}

inline const Signature DexFile::GetMethodSignature(const dex::MethodId& method_id) const {
  return Signature(this, GetProtoId(method_id.proto_idx_));
}

inline const Signature DexFile::GetProtoSignature(const dex::ProtoId& proto_id) const {
  return Signature(this, proto_id);
}

inline const char* DexFile::GetMethodName(const dex::MethodId& method_id) const {
  return StringDataByIdx(method_id.name_idx_);
}

inline const char* DexFile::GetMethodName(const dex::MethodId& method_id, uint32_t* utf_length)
    const {
  return StringDataAndUtf16LengthByIdx(method_id.name_idx_, utf_length);
}

inline const char* DexFile::GetMethodName(uint32_t idx) const {
  return StringDataByIdx(GetMethodId(idx).name_idx_);
}

inline const char* DexFile::GetMethodName(uint32_t idx, uint32_t* utf_length) const {
  return StringDataAndUtf16LengthByIdx(GetMethodId(idx).name_idx_, utf_length);
}

ALWAYS_INLINE
inline std::string_view DexFile::GetMethodNameView(const dex::MethodId& method_id) const {
  return StringViewByIdx(method_id.name_idx_);
}

ALWAYS_INLINE
inline std::string_view DexFile::GetMethodNameView(uint32_t idx) const {
  return GetMethodNameView(GetMethodId(idx));
}

inline const char* DexFile::GetMethodShorty(uint32_t idx) const {
  return StringDataByIdx(GetProtoId(GetMethodId(idx).proto_idx_).shorty_idx_);
}

inline const char* DexFile::GetMethodShorty(const dex::MethodId& method_id) const {
  return StringDataByIdx(GetProtoId(method_id.proto_idx_).shorty_idx_);
}

inline const char* DexFile::GetMethodShorty(const dex::MethodId& method_id, uint32_t* length)
    const {
  // Using the UTF16 length is safe here as shorties are guaranteed to be ASCII characters.
  return StringDataAndUtf16LengthByIdx(GetProtoId(method_id.proto_idx_).shorty_idx_, length);
}

inline std::string_view DexFile::GetMethodShortyView(const dex::MethodId& method_id) const {
  return GetShortyView(method_id.proto_idx_);
}

inline const char* DexFile::GetClassDescriptor(const dex::ClassDef& class_def) const {
  return StringByTypeIdx(class_def.class_idx_);
}

inline const char* DexFile::GetReturnTypeDescriptor(const dex::ProtoId& proto_id) const {
  return StringByTypeIdx(proto_id.return_type_idx_);
}

inline const char* DexFile::GetShorty(dex::ProtoIndex proto_idx) const {
  const dex::ProtoId& proto_id = GetProtoId(proto_idx);
  return StringDataByIdx(proto_id.shorty_idx_);
}

ALWAYS_INLINE
inline std::string_view DexFile::GetShortyView(dex::ProtoIndex proto_idx) const {
  return GetShortyView(GetProtoId(proto_idx));
}

ALWAYS_INLINE
inline std::string_view DexFile::GetShortyView(const dex::ProtoId& proto_id) const {
  uint32_t shorty_len;
  const char* shorty_data = StringDataAndUtf16LengthByIdx(proto_id.shorty_idx_, &shorty_len);
  DCHECK_EQ(shorty_data[shorty_len], '\0');  // For a shorty utf16 length == mutf8 length.
  return std::string_view(shorty_data, shorty_len);
}

inline const dex::TryItem* DexFile::GetTryItems(const DexInstructionIterator& code_item_end,
                                                uint32_t offset) {
  return reinterpret_cast<const dex::TryItem*>
      (RoundUp(reinterpret_cast<uintptr_t>(&code_item_end.Inst()), dex::TryItem::kAlignment)) +
          offset;
}

inline bool DexFile::StringEquals(const DexFile* df1, dex::StringIndex sidx1,
                                  const DexFile* df2, dex::StringIndex sidx2) {
  uint32_t s1_len;  // Note: utf16 length != mutf8 length.
  const char* s1_data = df1->StringDataAndUtf16LengthByIdx(sidx1, &s1_len);
  uint32_t s2_len;
  const char* s2_data = df2->StringDataAndUtf16LengthByIdx(sidx2, &s2_len);
  return (s1_len == s2_len) && (strcmp(s1_data, s2_data) == 0);
}

template<typename NewLocalCallback, typename IndexToStringData, typename TypeIndexToStringData>
bool DexFile::DecodeDebugLocalInfo(const uint8_t* stream,
                                   const std::string& location,
                                   const char* declaring_class_descriptor,
                                   const std::vector<const char*>& arg_descriptors,
                                   const std::string& method_name,
                                   bool is_static,
                                   uint16_t registers_size,
                                   uint16_t ins_size,
                                   uint16_t insns_size_in_code_units,
                                   const IndexToStringData& index_to_string_data,
                                   const TypeIndexToStringData& type_index_to_string_data,
                                   const NewLocalCallback& new_local_callback) {
  if (stream == nullptr) {
    return false;
  }
  std::vector<LocalInfo> local_in_reg(registers_size);

  uint16_t arg_reg = registers_size - ins_size;
  if (!is_static) {
    const char* descriptor = declaring_class_descriptor;
    local_in_reg[arg_reg].name_ = "this";
    local_in_reg[arg_reg].descriptor_ = descriptor;
    local_in_reg[arg_reg].signature_ = nullptr;
    local_in_reg[arg_reg].start_address_ = 0;
    local_in_reg[arg_reg].reg_ = arg_reg;
    local_in_reg[arg_reg].is_live_ = true;
    arg_reg++;
  }

  DecodeUnsignedLeb128(&stream);  // Line.
  uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
  uint32_t i;
  if (parameters_size != arg_descriptors.size()) {
    LOG(ERROR) << "invalid stream - problem with parameter iterator in " << location
               << " for method " << method_name;
    return false;
  }
  for (i = 0; i < parameters_size && i < arg_descriptors.size(); ++i) {
    if (arg_reg >= registers_size) {
      LOG(ERROR) << "invalid stream - arg reg >= reg size (" << arg_reg
                 << " >= " << registers_size << ") in " << location;
      return false;
    }
    uint32_t name_idx = DecodeUnsignedLeb128P1(&stream);
    const char* descriptor = arg_descriptors[i];
    local_in_reg[arg_reg].name_ = index_to_string_data(name_idx);
    local_in_reg[arg_reg].descriptor_ = descriptor;
    local_in_reg[arg_reg].signature_ = nullptr;
    local_in_reg[arg_reg].start_address_ = 0;
    local_in_reg[arg_reg].reg_ = arg_reg;
    local_in_reg[arg_reg].is_live_ = true;
    switch (*descriptor) {
      case 'D':
      case 'J':
        arg_reg += 2;
        break;
      default:
        arg_reg += 1;
        break;
    }
  }

  uint32_t address = 0;
  for (;;)  {
    uint8_t opcode = *stream++;
    switch (opcode) {
      case DBG_END_SEQUENCE:
        // Emit all variables which are still alive at the end of the method.
        for (uint16_t reg = 0; reg < registers_size; reg++) {
          if (local_in_reg[reg].is_live_) {
            local_in_reg[reg].end_address_ = insns_size_in_code_units;
            new_local_callback(local_in_reg[reg]);
          }
        }
        return true;
      case DBG_ADVANCE_PC:
        address += DecodeUnsignedLeb128(&stream);
        break;
      case DBG_ADVANCE_LINE:
        DecodeSignedLeb128(&stream);  // Line.
        break;
      case DBG_START_LOCAL:
      case DBG_START_LOCAL_EXTENDED: {
        uint16_t reg = DecodeUnsignedLeb128(&stream);
        if (reg >= registers_size) {
          LOG(ERROR) << "invalid stream - reg >= reg size (" << reg << " >= "
                     << registers_size << ") in " << location;
          return false;
        }

        uint32_t name_idx = DecodeUnsignedLeb128P1(&stream);
        uint16_t descriptor_idx = DecodeUnsignedLeb128P1(&stream);
        uint32_t signature_idx = dex::kDexNoIndex;
        if (opcode == DBG_START_LOCAL_EXTENDED) {
          signature_idx = DecodeUnsignedLeb128P1(&stream);
        }

        // Emit what was previously there, if anything
        if (local_in_reg[reg].is_live_) {
          local_in_reg[reg].end_address_ = address;
          // Parameters with generic types cannot be encoded in the debug_info_item header. So d8
          // encodes it as null in the header with start and end address as 0. There will be a
          // START_LOCAL_EXTENDED that will declare the parameter with correct signature
          // Debuggers get confused when they see empty ranges. So don't emit them.
          // See b/297843934 for more details.
          if (local_in_reg[reg].end_address_ != 0) {
            new_local_callback(local_in_reg[reg]);
          }
        }

        local_in_reg[reg].name_ = index_to_string_data(name_idx);
        local_in_reg[reg].descriptor_ = type_index_to_string_data(descriptor_idx);;
        local_in_reg[reg].signature_ = index_to_string_data(signature_idx);
        local_in_reg[reg].start_address_ = address;
        local_in_reg[reg].reg_ = reg;
        local_in_reg[reg].is_live_ = true;
        break;
      }
      case DBG_END_LOCAL: {
        uint16_t reg = DecodeUnsignedLeb128(&stream);
        if (reg >= registers_size) {
          LOG(ERROR) << "invalid stream - reg >= reg size (" << reg << " >= "
                     << registers_size << ") in " << location;
          return false;
        }
        // If the register is live, close it properly. Otherwise, closing an already
        // closed register is sloppy, but harmless if no further action is taken.
        if (local_in_reg[reg].is_live_) {
          local_in_reg[reg].end_address_ = address;
          new_local_callback(local_in_reg[reg]);
          local_in_reg[reg].is_live_ = false;
        }
        break;
      }
      case DBG_RESTART_LOCAL: {
        uint16_t reg = DecodeUnsignedLeb128(&stream);
        if (reg >= registers_size) {
          LOG(ERROR) << "invalid stream - reg >= reg size (" << reg << " >= "
                     << registers_size << ") in " << location;
          return false;
        }
        // If the register is live, the "restart" is superfluous,
        // and we don't want to mess with the existing start address.
        if (!local_in_reg[reg].is_live_) {
          local_in_reg[reg].start_address_ = address;
          local_in_reg[reg].is_live_ = true;
        }
        break;
      }
      case DBG_SET_PROLOGUE_END:
      case DBG_SET_EPILOGUE_BEGIN:
        break;
      case DBG_SET_FILE:
        DecodeUnsignedLeb128P1(&stream);  // name.
        break;
      default:
        address += (opcode - DBG_FIRST_SPECIAL) / DBG_LINE_RANGE;
        break;
    }
  }
}

template<typename NewLocalCallback>
bool DexFile::DecodeDebugLocalInfo(uint32_t registers_size,
                                   uint32_t ins_size,
                                   uint32_t insns_size_in_code_units,
                                   uint32_t debug_info_offset,
                                   bool is_static,
                                   uint32_t method_idx,
                                   const NewLocalCallback& new_local_callback) const {
  const uint8_t* const stream = GetDebugInfoStream(debug_info_offset);
  if (stream == nullptr) {
    return false;
  }
  std::vector<const char*> arg_descriptors;
  DexFileParameterIterator it(*this, GetMethodPrototype(GetMethodId(method_idx)));
  for (; it.HasNext(); it.Next()) {
    arg_descriptors.push_back(it.GetDescriptor());
  }
  return DecodeDebugLocalInfo(stream,
                              GetLocation(),
                              GetMethodDeclaringClassDescriptor(GetMethodId(method_idx)),
                              arg_descriptors,
                              this->PrettyMethod(method_idx),
                              is_static,
                              registers_size,
                              ins_size,
                              insns_size_in_code_units,
                              [this](uint32_t idx) {
                                return StringDataByIdx(dex::StringIndex(idx));
                              },
                              [this](uint32_t idx) {
                                return StringByTypeIdx(dex::TypeIndex(
                                    dchecked_integral_cast<uint16_t>(idx)));
                              },
                              new_local_callback);
}

template<typename DexDebugNewPosition, typename IndexToStringData>
bool DexFile::DecodeDebugPositionInfo(const uint8_t* stream,
                                      const IndexToStringData& index_to_string_data,
                                      const DexDebugNewPosition& position_functor) {
  if (stream == nullptr) {
    return false;
  }

  PositionInfo entry;
  entry.line_ = DecodeDebugInfoParameterNames(&stream, VoidFunctor());

  for (;;)  {
    uint8_t opcode = *stream++;
    switch (opcode) {
      case DBG_END_SEQUENCE:
        return true;  // end of stream.
      case DBG_ADVANCE_PC:
        entry.address_ += DecodeUnsignedLeb128(&stream);
        break;
      case DBG_ADVANCE_LINE:
        entry.line_ += DecodeSignedLeb128(&stream);
        break;
      case DBG_START_LOCAL:
        DecodeUnsignedLeb128(&stream);  // reg.
        DecodeUnsignedLeb128P1(&stream);  // name.
        DecodeUnsignedLeb128P1(&stream);  // descriptor.
        break;
      case DBG_START_LOCAL_EXTENDED:
        DecodeUnsignedLeb128(&stream);  // reg.
        DecodeUnsignedLeb128P1(&stream);  // name.
        DecodeUnsignedLeb128P1(&stream);  // descriptor.
        DecodeUnsignedLeb128P1(&stream);  // signature.
        break;
      case DBG_END_LOCAL:
      case DBG_RESTART_LOCAL:
        DecodeUnsignedLeb128(&stream);  // reg.
        break;
      case DBG_SET_PROLOGUE_END:
        entry.prologue_end_ = true;
        break;
      case DBG_SET_EPILOGUE_BEGIN:
        entry.epilogue_begin_ = true;
        break;
      case DBG_SET_FILE: {
        uint32_t name_idx = DecodeUnsignedLeb128P1(&stream);
        entry.source_file_ = index_to_string_data(name_idx);
        break;
      }
      default: {
        int adjopcode = opcode - DBG_FIRST_SPECIAL;
        entry.address_ += adjopcode / DBG_LINE_RANGE;
        entry.line_ += DBG_LINE_BASE + (adjopcode % DBG_LINE_RANGE);
        if (position_functor(entry)) {
          return true;  // early exit.
        }
        entry.prologue_end_ = false;
        entry.epilogue_begin_ = false;
        break;
      }
    }
  }
}

inline const CompactDexFile* DexFile::AsCompactDexFile() const {
  DCHECK(IsCompactDexFile());
  return down_cast<const CompactDexFile*>(this);
}

inline const StandardDexFile* DexFile::AsStandardDexFile() const {
  DCHECK(IsStandardDexFile());
  return down_cast<const StandardDexFile*>(this);
}

// Get the base of the encoded data for the given DexCode.
inline const uint8_t* DexFile::GetCatchHandlerData(const DexInstructionIterator& code_item_end,
                                                   uint32_t tries_size,
                                                   uint32_t offset) {
  const uint8_t* handler_data =
      reinterpret_cast<const uint8_t*>(GetTryItems(code_item_end, tries_size));
  return handler_data + offset;
}

inline IterationRange<ClassIterator> DexFile::GetClasses() const {
  return { ClassIterator(*this, 0u), ClassIterator(*this, NumClassDefs()) };
}

// Returns the line number
template <typename Visitor>
inline uint32_t DexFile::DecodeDebugInfoParameterNames(const uint8_t** debug_info,
                                                       const Visitor& visitor) {
  uint32_t line = DecodeUnsignedLeb128(debug_info);
  const uint32_t parameters_size = DecodeUnsignedLeb128(debug_info);
  for (uint32_t i = 0; i < parameters_size; ++i) {
    visitor(dex::StringIndex(DecodeUnsignedLeb128P1(debug_info)));
  }
  return line;
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_INL_H_
