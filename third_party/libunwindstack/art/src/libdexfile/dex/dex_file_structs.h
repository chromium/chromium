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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_STRUCTS_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_STRUCTS_H_

#include <android-base/logging.h>
#include <android-base/macros.h>

#include <inttypes.h>

#include "dex_file_types.h"
#include "modifiers.h"

namespace art {

class DexWriter;

namespace dex {

struct MapItem {
  uint16_t type_;
  uint16_t unused_;
  uint32_t size_;
  uint32_t offset_;
};

struct MapList {
  uint32_t size_;
  MapItem list_[1];

  size_t Size() const { return sizeof(uint32_t) + (size_ * sizeof(MapItem)); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MapList);
};

// Raw string_id_item.
struct StringId {
  uint32_t string_data_off_;  // offset in bytes from the base address

 private:
  DISALLOW_COPY_AND_ASSIGN(StringId);
};

// Raw type_id_item.
struct TypeId {
  dex::StringIndex descriptor_idx_;  // index into string_ids

 private:
  DISALLOW_COPY_AND_ASSIGN(TypeId);
};

// Raw field_id_item.
struct FieldId {
  dex::TypeIndex class_idx_;   // index into type_ids_ array for defining class
  dex::TypeIndex type_idx_;    // index into type_ids_ array for field type
  dex::StringIndex name_idx_;  // index into string_ids_ array for field name

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldId);
};

// Raw proto_id_item.
struct ProtoId {
  dex::StringIndex shorty_idx_;     // index into string_ids array for shorty descriptor
  dex::TypeIndex return_type_idx_;  // index into type_ids array for return type
  uint16_t pad_;                    // padding = 0
  uint32_t parameters_off_;         // file offset to type_list for parameter types

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtoId);
};

// Raw method_id_item.
struct MethodId {
  dex::TypeIndex class_idx_;   // index into type_ids_ array for defining class
  dex::ProtoIndex proto_idx_;  // index into proto_ids_ array for method prototype
  dex::StringIndex name_idx_;  // index into string_ids_ array for method name

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodId);
};

// Base code_item, compact dex and standard dex have different code item layouts.
struct CodeItem {
 protected:
  CodeItem() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeItem);
};

// Raw class_def_item.
struct ClassDef {
  dex::TypeIndex class_idx_;  // index into type_ids_ array for this class
  uint16_t pad1_;  // padding = 0
  uint32_t access_flags_;
  dex::TypeIndex superclass_idx_;  // index into type_ids_ array for superclass
  uint16_t pad2_;  // padding = 0
  uint32_t interfaces_off_;  // file offset to TypeList
  dex::StringIndex source_file_idx_;  // index into string_ids_ for source file name
  uint32_t annotations_off_;  // file offset to annotations_directory_item
  uint32_t class_data_off_;  // file offset to class_data_item
  uint32_t static_values_off_;  // file offset to EncodedArray

  // Returns the valid access flags, that is, Java modifier bits relevant to the ClassDef type
  // (class or interface). These are all in the lower 16b and do not contain runtime flags.
  uint32_t GetJavaAccessFlags() const {
    // Make sure that none of our runtime-only flags are set.
    static_assert((kAccValidClassFlags & kAccJavaFlagsMask) == kAccValidClassFlags,
                  "Valid class flags not a subset of Java flags");
    static_assert((kAccValidInterfaceFlags & kAccJavaFlagsMask) == kAccValidInterfaceFlags,
                  "Valid interface flags not a subset of Java flags");

    if ((access_flags_ & kAccInterface) != 0) {
      // Interface.
      return access_flags_ & kAccValidInterfaceFlags;
    } else {
      // Class.
      return access_flags_ & kAccValidClassFlags;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassDef);
};

// Raw type_item.
struct TypeItem {
  dex::TypeIndex type_idx_;  // index into type_ids section

 private:
  DISALLOW_COPY_AND_ASSIGN(TypeItem);
};

// Raw type_list.
class TypeList {
 public:
  uint32_t Size() const {
    return size_;
  }

  const TypeItem& GetTypeItem(uint32_t idx) const {
    DCHECK_LT(idx, this->size_);
    return this->list_[idx];
  }

  // Size in bytes of the part of the list that is common.
  static constexpr size_t GetHeaderSize() {
    return 4U;
  }

  // Size in bytes of the whole type list including all the stored elements.
  static constexpr size_t GetListSize(size_t count) {
    return GetHeaderSize() + sizeof(TypeItem) * count;
  }

 private:
  uint32_t size_;  // size of the list, in entries
  TypeItem list_[1];  // elements of the list
  DISALLOW_COPY_AND_ASSIGN(TypeList);
};

// raw method_handle_item
struct MethodHandleItem {
  uint16_t method_handle_type_;
  uint16_t reserved1_;            // Reserved for future use.
  uint16_t field_or_method_idx_;  // Field index for accessors, method index otherwise.
  uint16_t reserved2_;            // Reserved for future use.
 private:
  DISALLOW_COPY_AND_ASSIGN(MethodHandleItem);
};

// raw call_site_id_item
struct CallSiteIdItem {
  uint32_t data_off_;  // Offset into data section pointing to encoded array items.
 private:
  DISALLOW_COPY_AND_ASSIGN(CallSiteIdItem);
};

// Raw try_item.
struct TryItem {
  static constexpr size_t kAlignment = sizeof(uint32_t);

  uint32_t start_addr_;
  uint16_t insn_count_;
  uint16_t handler_off_;

 private:
  TryItem() = default;
  friend class ::art::DexWriter;
  DISALLOW_COPY_AND_ASSIGN(TryItem);
};

struct AnnotationsDirectoryItem {
  uint32_t class_annotations_off_;
  uint32_t fields_size_;
  uint32_t methods_size_;
  uint32_t parameters_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationsDirectoryItem);
};

struct FieldAnnotationsItem {
  uint32_t field_idx_;
  uint32_t annotations_off_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FieldAnnotationsItem);
};

struct MethodAnnotationsItem {
  uint32_t method_idx_;
  uint32_t annotations_off_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MethodAnnotationsItem);
};

struct ParameterAnnotationsItem {
  uint32_t method_idx_;
  uint32_t annotations_off_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParameterAnnotationsItem);
};

struct AnnotationSetRefItem {
  uint32_t annotations_off_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefItem);
};

struct AnnotationSetRefList {
  uint32_t size_;
  AnnotationSetRefItem list_[1];

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetRefList);
};

struct AnnotationSetItem {
  uint32_t size_;
  uint32_t entries_[1];

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationSetItem);
};

struct AnnotationItem {
  uint8_t visibility_;
  uint8_t annotation_[1];

 private:
  DISALLOW_COPY_AND_ASSIGN(AnnotationItem);
};

struct HiddenapiClassData {
  uint32_t size_;             // total size of the item
  uint32_t flags_offset_[1];  // array of offsets from the beginning of this item,
                              // indexed by class def index

  // Returns a pointer to the beginning of a uleb128-stream of hiddenapi
  // flags for a class def of given index. Values are in the same order
  // as fields/methods in the class data. Returns null if the class does
  // not have class data.
  const uint8_t* GetFlagsPointer(uint32_t class_def_idx) const {
    if (flags_offset_[class_def_idx] == 0) {
      return nullptr;
    } else {
      return reinterpret_cast<const uint8_t*>(this) + flags_offset_[class_def_idx];
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HiddenapiClassData);
};

}  // namespace dex
}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_STRUCTS_H_
