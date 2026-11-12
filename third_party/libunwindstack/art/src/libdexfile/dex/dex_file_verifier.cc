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

#include "dex_file_verifier.h"

#include <algorithm>
#include <alloca.h>
#include <bitset>
#include <limits>
#include <memory>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android-base/stringprintf.h"

#include "base/hash_map.h"
#include "base/leb128.h"
#include "base/safe_map.h"
#include "class_accessor-inl.h"
#include "code_item_accessors-inl.h"
#include "descriptors_names.h"
#include "dex_file-inl.h"
#include "dex_file_types.h"
#include "modifiers.h"
#include "utf-inl.h"

namespace art {
namespace dex {

using android::base::StringAppendV;
using android::base::StringPrintf;

namespace {

constexpr uint32_t kTypeIdLimit = std::numeric_limits<uint16_t>::max();

constexpr bool IsValidOrNoTypeId(uint16_t low, uint16_t high) {
  return (high == 0) || ((high == 0xffffU) && (low == 0xffffU));
}

constexpr bool IsValidTypeId([[maybe_unused]] uint16_t low, uint16_t high) { return (high == 0); }

constexpr uint32_t MapTypeToBitMask(DexFile::MapItemType map_item_type) {
  switch (map_item_type) {
    case DexFile::kDexTypeHeaderItem:               return 1 << 0;
    case DexFile::kDexTypeStringIdItem:             return 1 << 1;
    case DexFile::kDexTypeTypeIdItem:               return 1 << 2;
    case DexFile::kDexTypeProtoIdItem:              return 1 << 3;
    case DexFile::kDexTypeFieldIdItem:              return 1 << 4;
    case DexFile::kDexTypeMethodIdItem:             return 1 << 5;
    case DexFile::kDexTypeClassDefItem:             return 1 << 6;
    case DexFile::kDexTypeCallSiteIdItem:           return 1 << 7;
    case DexFile::kDexTypeMethodHandleItem:         return 1 << 8;
    case DexFile::kDexTypeMapList:                  return 1 << 9;
    case DexFile::kDexTypeTypeList:                 return 1 << 10;
    case DexFile::kDexTypeAnnotationSetRefList:     return 1 << 11;
    case DexFile::kDexTypeAnnotationSetItem:        return 1 << 12;
    case DexFile::kDexTypeClassDataItem:            return 1 << 13;
    case DexFile::kDexTypeCodeItem:                 return 1 << 14;
    case DexFile::kDexTypeStringDataItem:           return 1 << 15;
    case DexFile::kDexTypeDebugInfoItem:            return 1 << 16;
    case DexFile::kDexTypeAnnotationItem:           return 1 << 17;
    case DexFile::kDexTypeEncodedArrayItem:         return 1 << 18;
    case DexFile::kDexTypeAnnotationsDirectoryItem: return 1 << 19;
    case DexFile::kDexTypeHiddenapiClassData:       return 1 << 20;
  }
  return 0;
}

constexpr bool IsDataSectionType(DexFile::MapItemType map_item_type) {
  switch (map_item_type) {
    case DexFile::kDexTypeHeaderItem:
    case DexFile::kDexTypeStringIdItem:
    case DexFile::kDexTypeTypeIdItem:
    case DexFile::kDexTypeProtoIdItem:
    case DexFile::kDexTypeFieldIdItem:
    case DexFile::kDexTypeMethodIdItem:
    case DexFile::kDexTypeClassDefItem:
      return false;
    case DexFile::kDexTypeCallSiteIdItem:
    case DexFile::kDexTypeMethodHandleItem:
    case DexFile::kDexTypeMapList:
    case DexFile::kDexTypeTypeList:
    case DexFile::kDexTypeAnnotationSetRefList:
    case DexFile::kDexTypeAnnotationSetItem:
    case DexFile::kDexTypeClassDataItem:
    case DexFile::kDexTypeCodeItem:
    case DexFile::kDexTypeStringDataItem:
    case DexFile::kDexTypeDebugInfoItem:
    case DexFile::kDexTypeAnnotationItem:
    case DexFile::kDexTypeEncodedArrayItem:
    case DexFile::kDexTypeAnnotationsDirectoryItem:
    case DexFile::kDexTypeHiddenapiClassData:
      return true;
  }
  return true;
}

// Fields and methods may have only one of public/protected/private.
ALWAYS_INLINE
constexpr bool CheckAtMostOneOfPublicProtectedPrivate(uint32_t flags) {
  // Semantically we want 'return POPCOUNT(flags & kAcc) <= 1;'.
  static_assert(IsPowerOfTwo(0), "0 not marked as power of two");
  static_assert(IsPowerOfTwo(kAccPublic), "kAccPublic not marked as power of two");
  static_assert(IsPowerOfTwo(kAccProtected), "kAccProtected not marked as power of two");
  static_assert(IsPowerOfTwo(kAccPrivate), "kAccPrivate not marked as power of two");
  return IsPowerOfTwo(flags & (kAccPublic | kAccProtected | kAccPrivate));
}

}  // namespace

// Note: the anonymous namespace would be nice, but we need friend access into accessors.

class DexFileVerifier {
 public:
  DexFileVerifier(const DexFile* dex_file, const char* location, bool verify_checksum)
      : dex_file_(dex_file),
        offset_base_address_(dex_file->DataBegin()),
        size_(dex_file->DataSize()),
        location_(location),
        verify_checksum_(verify_checksum),
        header_(&dex_file->GetHeader()),
        ptr_(nullptr),
        previous_item_(nullptr),
        init_indices_{std::numeric_limits<size_t>::max(),
                      std::numeric_limits<size_t>::max(),
                      std::numeric_limits<size_t>::max(),
                      std::numeric_limits<size_t>::max()} {
    CHECK(!dex_file->IsCompactDexFile()) << "Not supported";
  }

  bool Verify();

  const std::string& FailureReason() const {
    return failure_reason_;
  }

 private:
  template <class T = uint8_t>
  ALWAYS_INLINE const T* OffsetToPtr(size_t offset) {
    DCHECK_GE(offset, static_cast<size_t>(dex_file_->Begin() - offset_base_address_));
    DCHECK_LE(offset, size_);
    return reinterpret_cast<const T*>(offset_base_address_ + offset);
  }

  ALWAYS_INLINE size_t PtrToOffset(const void* ptr) {
    DCHECK_GE(ptr, dex_file_->Begin());
    DCHECK_LE(ptr, EndOfFile());
    return reinterpret_cast<const uint8_t*>(ptr) - offset_base_address_;
  }

  // Converts the pointer `ptr` into `offset`.
  // Returns `true` if the offset is within the bounds of the container.
  // TODO: Try to remove this overload. Avoid creating invalid pointers.
  ALWAYS_INLINE WARN_UNUSED bool PtrToOffset(const void* ptr, /*out*/ size_t* offset) {
    *offset = reinterpret_cast<const uint8_t*>(ptr) - offset_base_address_;
    return *offset <= size_;
  }

  ALWAYS_INLINE const uint8_t* EndOfFile() {
    return OffsetToPtr(size_);
  }

  // Helper functions to retrieve names from the dex file. We do not want to rely on DexFile
  // functionality, as we're still verifying the dex file.

  std::string GetString(dex::StringIndex string_idx) {
    // All sources of the `string_idx` have already been checked in CheckIntraSection().
    DCHECK_LT(string_idx.index_, header_->string_ids_size_);
    const dex::StringId& string_id =
        OffsetToPtr<dex::StringId>(header_->string_ids_off_)[string_idx.index_];

    // The string offset has been checked at the start of `CheckInterSection()`
    // to point to a string data item checked by `CheckIntraSection()`.
    const uint8_t* ptr = OffsetToPtr(string_id.string_data_off_);
    DecodeUnsignedLeb128(&ptr);  // Ignore the result.
    return reinterpret_cast<const char*>(ptr);
  }

  std::string GetClass(dex::TypeIndex class_idx) {
    // All sources of `class_idx` have already been checked in CheckIntraSection().
    CHECK_LT(class_idx.index_, header_->type_ids_size_);

    const dex::TypeId& type_id = OffsetToPtr<dex::TypeId>(header_->type_ids_off_)[class_idx.index_];

    // The `type_id->descriptor_idx_` has already been checked in CheckIntraTypeIdItem().
    // However, it may not have been checked to be a valid descriptor, so return the raw
    // string without converting with `PrettyDescriptor()`.
    return GetString(type_id.descriptor_idx_);
  }

  std::string GetFieldDescription(uint32_t idx) {
    // The `idx` has already been checked in `DexFileVerifier::CheckIntraClassDataItemFields()`.
    CHECK_LT(idx, header_->field_ids_size_);

    const dex::FieldId& field_id = OffsetToPtr<dex::FieldId>(header_->field_ids_off_)[idx];

    // Indexes in `*field_id` have already been checked in CheckIntraFieldIdItem().
    std::string class_name = GetClass(field_id.class_idx_);
    std::string field_name = GetString(field_id.name_idx_);
    return class_name + "." + field_name;
  }

  std::string GetMethodDescription(uint32_t idx) {
    // The `idx` has already been checked in `DexFileVerifier::CheckIntraClassDataItemMethods()`.
    CHECK_LT(idx, header_->method_ids_size_);

    const dex::MethodId& method_id = OffsetToPtr<dex::MethodId>(header_->method_ids_off_)[idx];

    // Indexes in `method_id` have already been checked in CheckIntraMethodIdItem().
    std::string class_name = GetClass(method_id.class_idx_);
    std::string method_name = GetString(method_id.name_idx_);
    return class_name + "." + method_name;
  }

  bool CheckShortyDescriptorMatch(char shorty_char, const char* descriptor, bool is_return_type);
  bool CheckListSize(const void* start, size_t count, size_t element_size, const char* label);
  // Check a list. The head is assumed to be at *ptr, and elements to be of size element_size. If
  // successful, the ptr will be moved forward the amount covered by the list.
  bool CheckList(size_t element_size, const char* label, const uint8_t* *ptr);
  // Checks:
  //   * the offset is zero (when size is zero),
  //   * the offset falls within the area claimed by the file,
  //   * the offset + size also falls within the area claimed by the file, and
  //   * the alignment of the section
  bool CheckValidOffsetAndSize(uint32_t offset, uint32_t size, size_t alignment, const char* label);
  // Checks whether the size is less than the limit.
  ALWAYS_INLINE bool CheckSizeLimit(uint32_t size, uint32_t limit, const char* label) {
    if (size > limit) {
      ErrorStringPrintf("Size(%u) should not exceed limit(%u) for %s.", size, limit, label);
      return false;
    }
    return true;
  }
  ALWAYS_INLINE bool CheckIndex(uint32_t field, uint32_t limit, const char* label) {
    if (UNLIKELY(field >= limit)) {
      ErrorStringPrintf("Bad index for %s: %x >= %x", label, field, limit);
      return false;
    }
    return true;
  }

  bool CheckHeader();
  bool CheckMap();

  uint32_t ReadUnsignedLittleEndian(uint32_t size) {
    uint32_t result = 0;
    if (LIKELY(CheckListSize(ptr_, size, sizeof(uint8_t), "encoded_value"))) {
      for (uint32_t i = 0; i < size; i++) {
        result |= ((uint32_t) *(ptr_++)) << (i * 8);
      }
    }
    return result;
  }
  bool CheckAndGetHandlerOffsets(const dex::CodeItem* code_item,
                                 uint32_t* handler_offsets, uint32_t handlers_size);
  bool CheckClassDataItemField(uint32_t idx,
                               uint32_t access_flags,
                               uint32_t class_access_flags,
                               dex::TypeIndex class_type_index);
  bool CheckClassDataItemMethod(uint32_t idx,
                                uint32_t access_flags,
                                uint32_t class_access_flags,
                                dex::TypeIndex class_type_index,
                                uint32_t code_offset,
                                bool expect_direct);
  ALWAYS_INLINE
  bool CheckOrder(const char* type_descr, uint32_t curr_index, uint32_t prev_index) {
    if (UNLIKELY(curr_index < prev_index)) {
      ErrorStringPrintf("out-of-order %s indexes %" PRIu32 " and %" PRIu32,
                        type_descr,
                        prev_index,
                        curr_index);
      return false;
    }
    return true;
  }
  bool CheckStaticFieldTypes(const dex::ClassDef& class_def);

  bool CheckPadding(uint32_t aligned_offset, DexFile::MapItemType type);
  bool CheckEncodedValue();
  bool CheckEncodedArray();
  bool CheckEncodedAnnotation();

  bool CheckIntraTypeIdItem();
  bool CheckIntraProtoIdItem();
  bool CheckIntraFieldIdItem();
  bool CheckIntraMethodIdItem();
  bool CheckIntraClassDefItem(uint32_t class_def_index);
  bool CheckIntraMethodHandleItem();
  bool CheckIntraTypeList();
  // Check all fields of the given type, reading `encoded_field` entries from `ptr_`.
  template <bool kStatic>
  bool CheckIntraClassDataItemFields(size_t count);
  // Check direct or virtual methods, reading `encoded_method` entries from `ptr_`.
  // Check virtual methods against duplicates with direct methods.
  bool CheckIntraClassDataItemMethods(size_t num_methods,
                                      ClassAccessor::Method* direct_methods,
                                      size_t num_direct_methods);
  bool CheckIntraClassDataItem();

  bool CheckIntraCodeItem();
  bool CheckIntraStringDataItem();
  bool CheckIntraDebugInfoItem();
  bool CheckIntraAnnotationItem();
  bool CheckIntraAnnotationsDirectoryItem();
  bool CheckIntraHiddenapiClassData();

  template <DexFile::MapItemType kType>
  bool CheckIntraSectionIterate(uint32_t count);
  template <DexFile::MapItemType kType>
  bool CheckIntraIdSection(size_t offset, uint32_t count);
  template <DexFile::MapItemType kType>
  bool CheckIntraDataSection(size_t offset, uint32_t count);
  bool CheckIntraSection();

  bool CheckOffsetToTypeMap(size_t offset, uint16_t type);

  // Returns kDexNoIndex if there are no fields/methods, otherwise a 16-bit type index.
  uint32_t FindFirstClassDataDefiner(const ClassAccessor& accessor);
  uint32_t FindFirstAnnotationsDirectoryDefiner(const uint8_t* ptr);

  bool CheckInterHiddenapiClassData();
  bool CheckInterStringIdItem();
  bool CheckInterTypeIdItem();
  bool CheckInterProtoIdItem();
  bool CheckInterFieldIdItem();
  bool CheckInterMethodIdItem();
  bool CheckInterClassDefItem();
  bool CheckInterCallSiteIdItem();
  bool CheckInterAnnotationSetRefList();
  bool CheckInterAnnotationSetItem();
  bool CheckInterClassDataItem();
  bool CheckInterAnnotationsDirectoryItem();

  bool CheckInterSectionIterate(size_t offset, uint32_t count, DexFile::MapItemType type);
  bool CheckInterSection();

  void ErrorStringPrintf(const char* fmt, ...)
      __attribute__((__format__(__printf__, 2, 3))) COLD_ATTR {
    va_list ap;
    va_start(ap, fmt);
    DCHECK(failure_reason_.empty()) << failure_reason_;
    failure_reason_ = StringPrintf("Failure to verify dex file '%s': ", location_);
    StringAppendV(&failure_reason_, fmt, ap);
    va_end(ap);
  }
  bool FailureReasonIsSet() const { return failure_reason_.size() != 0; }

  // Check validity of the given access flags, interpreted for a field in the context of a class
  // with the given second access flags.
  bool CheckFieldAccessFlags(uint32_t idx,
                             uint32_t field_access_flags,
                             uint32_t class_access_flags,
                             std::string* error_message);

  // Check validity of the given method and access flags, in the context of a class with the given
  // second access flags.
  bool CheckMethodAccessFlags(uint32_t method_index,
                              uint32_t method_access_flags,
                              uint32_t class_access_flags,
                              uint32_t constructor_flags_by_name,
                              bool has_code,
                              bool expect_direct,
                              std::string* error_message);

  // Check validity of given method if it's a constructor or class initializer.
  bool CheckConstructorProperties(uint32_t method_index, uint32_t constructor_flags);

  void FindStringRangesForMethodNames();

  template <typename ExtraCheckFn>
  bool VerifyTypeDescriptor(dex::TypeIndex idx, const char* error_msg, ExtraCheckFn extra_check);

  const DexFile* const dex_file_;
  const uint8_t* const offset_base_address_;
  const size_t size_;
  ArrayRef<const uint8_t> data_;  // The "data" section of the dex file.
  const char* const location_;
  const bool verify_checksum_;
  const DexFile::Header* const header_;
  uint32_t dex_version_ = 0;

  struct OffsetTypeMapEmptyFn {
    // Make a hash map slot empty by making the offset 0. Offset 0 is a valid dex file offset that
    // is in the offset of the dex file header. However, we only store data section items in the
    // map, and these are after the header.
    void MakeEmpty(std::pair<uint32_t, uint16_t>& pair) const {
      pair.first = 0u;
    }
    // Check if a hash map slot is empty.
    bool IsEmpty(const std::pair<uint32_t, uint16_t>& pair) const {
      return pair.first == 0;
    }
  };
  struct OffsetTypeMapHashCompareFn {
    // Hash function for offset.
    size_t operator()(const uint32_t key) const {
      return key;
    }
    // std::equal function for offset.
    bool operator()(const uint32_t a, const uint32_t b) const {
      return a == b;
    }
  };
  // Map from offset to dex file type, HashMap for performance reasons.
  HashMap<uint32_t,
          uint16_t,
          OffsetTypeMapEmptyFn,
          OffsetTypeMapHashCompareFn,
          OffsetTypeMapHashCompareFn> offset_to_type_map_;
  const uint8_t* ptr_;
  const void* previous_item_;

  std::string failure_reason_;

  // Cached string indices for "interesting" entries wrt/ method names. Will be populated by
  // FindStringRangesForMethodNames (which is automatically called before verifying the
  // classdataitem section).
  //
  // Strings starting with '<' are in the range
  //    [angle_bracket_start_index_,angle_bracket_end_index_).
  // angle_init_angle_index_ and angle_clinit_angle_index_ denote the indices of "<init>" and
  // "<clinit>", respectively. If any value is not found, the corresponding index will be larger
  // than any valid string index for this dex file.
  struct {
    size_t angle_bracket_start_index;
    size_t angle_bracket_end_index;
    size_t angle_init_angle_index;
    size_t angle_clinit_angle_index;
  } init_indices_;

  // A bitvector for verified type descriptors. Each bit corresponds to a type index. A set
  // bit denotes that the descriptor has been verified wrt/ IsValidDescriptor.
  std::vector<char> verified_type_descriptors_;

  // Set of type ids for which there are ClassDef elements in the dex file. Using a bitset
  // avoids all allocations. The bitset should be implemented as 8K of storage, which is
  // tight enough for all callers.
  std::bitset<kTypeIdLimit + 1> defined_classes_;

  // Class definition indexes, valid only if corresponding `defined_classes_[.]` is true.
  std::vector<uint16_t> defined_class_indexes_;
};

template <typename ExtraCheckFn>
bool DexFileVerifier::VerifyTypeDescriptor(dex::TypeIndex idx,
                                           const char* error_msg,
                                           ExtraCheckFn extra_check) {
  // All sources of the `idx` have already been checked in CheckIntraSection().
  DCHECK_LT(idx.index_, header_->type_ids_size_);

  char cached_char = verified_type_descriptors_[idx.index_];
  if (cached_char != 0) {
    if (!extra_check(cached_char)) {
      const char* descriptor = dex_file_->StringByTypeIdx(idx);
      ErrorStringPrintf("%s: '%s'", error_msg, descriptor);
      return false;
    }
    return true;
  }

  const char* descriptor = dex_file_->StringByTypeIdx(idx);
  if (UNLIKELY(!IsValidDescriptor(descriptor))) {
    ErrorStringPrintf("%s: '%s'", error_msg, descriptor);
    return false;
  }
  verified_type_descriptors_[idx.index_] = descriptor[0];

  if (!extra_check(descriptor[0])) {
    ErrorStringPrintf("%s: '%s'", error_msg, descriptor);
    return false;
  }
  return true;
}

bool DexFileVerifier::CheckShortyDescriptorMatch(char shorty_char, const char* descriptor,
                                                bool is_return_type) {
  switch (shorty_char) {
    case 'V':
      if (UNLIKELY(!is_return_type)) {
        ErrorStringPrintf("Invalid use of void");
        return false;
      }
      FALLTHROUGH_INTENDED;
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    case 'S':
    case 'Z':
      if (UNLIKELY((descriptor[0] != shorty_char) || (descriptor[1] != '\0'))) {
        ErrorStringPrintf("Shorty vs. primitive type mismatch: '%c', '%s'",
                          shorty_char, descriptor);
        return false;
      }
      break;
    case 'L':
      if (UNLIKELY((descriptor[0] != 'L') && (descriptor[0] != '['))) {
        ErrorStringPrintf("Shorty vs. type mismatch: '%c', '%s'", shorty_char, descriptor);
        return false;
      }
      break;
    default:
      ErrorStringPrintf("Bad shorty character: '%c'", shorty_char);
      return false;
  }
  return true;
}

bool DexFileVerifier::CheckListSize(const void* start, size_t count, size_t elem_size,
                                    const char* label) {
  // Check that element size is not 0.
  DCHECK_NE(elem_size, 0U);

  size_t offset;
  if (!PtrToOffset(start, &offset)) {
    ErrorStringPrintf("Offset beyond end of file for %s: %zx to %zx", label, offset, size_);
    return false;
  }

  // Calculate the number of elements that fit until the end of file,
  // rather than calculating the end of the range as that could overflow.
  size_t max_elements = (size_ - offset) / elem_size;
  if (UNLIKELY(max_elements < count)) {
    ErrorStringPrintf(
        "List too large for %s: %zx+%zu*%zu > %zx", label, offset, count, elem_size, size_);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckList(size_t element_size, const char* label, const uint8_t* *ptr) {
  // Check that the list is available. The first 4B are the count.
  if (!CheckListSize(*ptr, 1, 4U, label)) {
    return false;
  }

  uint32_t count = *reinterpret_cast<const uint32_t*>(*ptr);
  if (count > 0) {
    if (!CheckListSize(*ptr + 4, count, element_size, label)) {
      return false;
    }
  }

  *ptr += 4 + count * element_size;
  return true;
}

bool DexFileVerifier::CheckValidOffsetAndSize(uint32_t offset,
                                              uint32_t size,
                                              size_t alignment,
                                              const char* label) {
  if (size == 0) {
    if (offset != 0) {
      ErrorStringPrintf("Offset(%d) should be zero when size is zero for %s.", offset, label);
      return false;
    }
    return true;
  }
  size_t hdr_offset = PtrToOffset(header_);
  if (offset < hdr_offset) {
    ErrorStringPrintf("Offset(%d) should be after header(%zu) for %s.", offset, hdr_offset, label);
    return false;
  }
  if (size_ <= offset) {
    ErrorStringPrintf("Offset(%d) should be within file size(%zu) for %s.", offset, size_, label);
    return false;
  }
  // Check that offset + size is within the file size. Note that we use `<` to allow the section to
  // end at the same point as the file. Check written as a subtraction to be safe from overfow.
  if (size_ - offset < size) {
    ErrorStringPrintf(
        "Section end(%d) should be within file size(%zu) for %s.", offset + size, size_, label);
    return false;
  }
  if (alignment != 0 && !IsAlignedParam(offset, alignment)) {
    ErrorStringPrintf("Offset(%d) should be aligned by %zu for %s.", offset, alignment, label);
    return false;
  }
  return true;
}

bool DexFileVerifier::CheckHeader() {
  // Check magic.
  size_t size = dex_file_->GetContainer()->End() - dex_file_->Begin();
  if (size < sizeof(DexFile::Header)) {
    ErrorStringPrintf("Empty or truncated file.");
    return false;
  }
  if (!StandardDexFile::IsMagicValid(header_->magic_.data())) {
    ErrorStringPrintf("Bad file magic");
    return false;
  }
  if (!StandardDexFile::IsVersionValid(header_->magic_.data())) {
    ErrorStringPrintf("Unknown dex version");
    return false;
  }
  dex_version_ = header_->GetVersion();

  // Check file size from the header.
  size_t file_size = header_->file_size_;
  size_t header_size = (dex_version_ >= 41) ? sizeof(DexFile::HeaderV41) : sizeof(DexFile::Header);
  if (file_size < header_size) {
    ErrorStringPrintf("Bad file size (%zu, expected at least %zu)", file_size, header_size);
    return false;
  }
  if (file_size > size) {
    ErrorStringPrintf("Bad file size (%zu, expected at most %zu)", file_size, size);
    return false;
  }
  CHECK_GE(size, header_size);  // Implied by the two checks above.

  // Check header size.
  if (header_->header_size_ != header_size) {
    ErrorStringPrintf("Bad header size: %ud expected %zud", header_->header_size_, header_size);
    return false;
  }

  // Check the endian.
  if (header_->endian_tag_ != DexFile::kDexEndianConstant) {
    ErrorStringPrintf("Unexpected endian_tag: %x", header_->endian_tag_);
    return false;
  }

  // Compute and verify the checksum in the header.
  uint32_t adler_checksum = dex_file_->CalculateChecksum();
  if (adler_checksum != header_->checksum_) {
    if (verify_checksum_) {
      ErrorStringPrintf("Bad checksum (%08x, expected %08x)", adler_checksum, header_->checksum_);
      return false;
    } else {
      LOG(WARNING) << StringPrintf(
          "Ignoring bad checksum (%08x, expected %08x)", adler_checksum, header_->checksum_);
    }
  }

  if (dex_version_ >= 41) {
    auto headerV41 = reinterpret_cast<const DexFile::HeaderV41*>(header_);
    if (headerV41->container_size_ <= headerV41->header_offset_) {
      ErrorStringPrintf("Dex container is too small: size=%ud header_offset=%ud",
                        headerV41->container_size_,
                        headerV41->header_offset_);
      return false;
    }
    uint32_t remainder = headerV41->container_size_ - headerV41->header_offset_;
    if (headerV41->file_size_ > remainder) {
      ErrorStringPrintf(
          "Header file_size(%ud) is past multi-dex size(%ud)", headerV41->file_size_, remainder);
      return false;
    }
  }

  // Check that all offsets are inside the file.
  bool ok =
      CheckValidOffsetAndSize(header_->link_off_,
                              header_->link_size_,
                              /* alignment= */ 0,
                              "link") &&
      CheckValidOffsetAndSize(header_->map_off_,
                              sizeof(dex::MapList),
                              /* alignment= */ 4,
                              "map") &&
      CheckValidOffsetAndSize(header_->string_ids_off_,
                              header_->string_ids_size_,
                              /* alignment= */ 4,
                              "string-ids") &&
      CheckValidOffsetAndSize(header_->type_ids_off_,
                              header_->type_ids_size_,
                              /* alignment= */ 4,
                              "type-ids") &&
      CheckSizeLimit(header_->type_ids_size_, DexFile::kDexNoIndex16, "type-ids") &&
      CheckValidOffsetAndSize(header_->proto_ids_off_,
                              header_->proto_ids_size_,
                              /* alignment= */ 4,
                              "proto-ids") &&
      CheckSizeLimit(header_->proto_ids_size_, DexFile::kDexNoIndex16, "proto-ids") &&
      CheckValidOffsetAndSize(header_->field_ids_off_,
                              header_->field_ids_size_,
                              /* alignment= */ 4,
                              "field-ids") &&
      CheckValidOffsetAndSize(header_->method_ids_off_,
                              header_->method_ids_size_,
                              /* alignment= */ 4,
                              "method-ids") &&
      CheckValidOffsetAndSize(header_->class_defs_off_,
                              header_->class_defs_size_,
                              /* alignment= */ 4,
                              "class-defs") &&
      CheckValidOffsetAndSize(header_->data_off_,
                              header_->data_size_,
                              // Unaligned, spec doesn't talk about it, even though size
                              // is supposed to be a multiple of 4.
                              /* alignment= */ 0,
                              "data");

  if (ok) {
    data_ = (dex_version_ >= 41)
        ? ArrayRef<const uint8_t>(dex_file_->Begin(), EndOfFile() - dex_file_->Begin())
        : ArrayRef<const uint8_t>(OffsetToPtr(header_->data_off_), header_->data_size_);
  }
  return ok;
}

bool DexFileVerifier::CheckMap() {
  const dex::MapList* map = OffsetToPtr<dex::MapList>(header_->map_off_);
  // Check that map list content is available.
  if (!CheckListSize(map, 1, sizeof(dex::MapList), "maplist content")) {
    return false;
  }

  const dex::MapItem* item = map->list_;

  uint32_t count = map->size_;
  uint32_t last_offset = 0;
  uint32_t last_type = 0;
  uint32_t data_item_count = 0;
  uint32_t data_items_left = data_.size();
  uint32_t used_bits = 0;

  // Check the validity of the size of the map list.
  if (!CheckListSize(item, count, sizeof(dex::MapItem), "map size")) {
    return false;
  }

  // Check the items listed in the map.
  for (uint32_t i = 0; i < count; i++) {
    if (UNLIKELY(last_offset >= item->offset_ && i != 0)) {
      ErrorStringPrintf("Out of order map item: %x then %x for type %x last type was %x",
                        last_offset,
                        item->offset_,
                        static_cast<uint32_t>(item->type_),
                        last_type);
      return false;
    }
    if (UNLIKELY(item->offset_ >= size_)) {
      ErrorStringPrintf("Map item after end of file: %x, size %zx", item->offset_, size_);
      return false;
    }

    DexFile::MapItemType item_type = static_cast<DexFile::MapItemType>(item->type_);
    if (IsDataSectionType(item_type)) {
      uint32_t icount = item->size_;
      if (UNLIKELY(icount > data_items_left)) {
        ErrorStringPrintf("Too many items in data section: %ud item_type %zx",
                          data_item_count + icount,
                          static_cast<size_t>(item_type));
        return false;
      }
      data_items_left -= icount;
      data_item_count += icount;
    }

    uint32_t bit = MapTypeToBitMask(item_type);

    if (UNLIKELY(bit == 0)) {
      ErrorStringPrintf("Unknown map section type %x", item->type_);
      return false;
    }

    if (UNLIKELY((used_bits & bit) != 0)) {
      ErrorStringPrintf("Duplicate map section of type %x", item->type_);
      return false;
    }

    used_bits |= bit;
    last_offset = item->offset_;
    last_type = item->type_;
    item++;
  }

  // Check for missing sections in the map.
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeHeaderItem)) == 0)) {
    ErrorStringPrintf("Map is missing header entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeMapList)) == 0)) {
    ErrorStringPrintf("Map is missing map_list entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeStringIdItem)) == 0 &&
               ((header_->string_ids_off_ != 0) || (header_->string_ids_size_ != 0)))) {
    ErrorStringPrintf("Map is missing string_ids entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeTypeIdItem)) == 0 &&
               ((header_->type_ids_off_ != 0) || (header_->type_ids_size_ != 0)))) {
    ErrorStringPrintf("Map is missing type_ids entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeProtoIdItem)) == 0 &&
               ((header_->proto_ids_off_ != 0) || (header_->proto_ids_size_ != 0)))) {
    ErrorStringPrintf("Map is missing proto_ids entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeFieldIdItem)) == 0 &&
               ((header_->field_ids_off_ != 0) || (header_->field_ids_size_ != 0)))) {
    ErrorStringPrintf("Map is missing field_ids entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeMethodIdItem)) == 0 &&
               ((header_->method_ids_off_ != 0) || (header_->method_ids_size_ != 0)))) {
    ErrorStringPrintf("Map is missing method_ids entry");
    return false;
  }
  if (UNLIKELY((used_bits & MapTypeToBitMask(DexFile::kDexTypeClassDefItem)) == 0 &&
               ((header_->class_defs_off_ != 0) || (header_->class_defs_size_ != 0)))) {
    ErrorStringPrintf("Map is missing class_defs entry");
    return false;
  }
  return true;
}

#define DECODE_UNSIGNED_CHECKED_FROM(ptr, var)                        \
  uint32_t var;                                                       \
  if (!DecodeUnsignedLeb128Checked(&(ptr), EndOfFile(), &(var))) {    \
    ErrorStringPrintf("Read out of bounds");                          \
    return false;                                                     \
  }

#define DECODE_SIGNED_CHECKED_FROM(ptr, var)                        \
  int32_t var;                                                      \
  if (!DecodeSignedLeb128Checked(&(ptr), EndOfFile(), &(var))) {    \
    ErrorStringPrintf("Read out of bounds");                        \
    return false;                                                   \
  }

bool DexFileVerifier::CheckAndGetHandlerOffsets(const dex::CodeItem* code_item,
                                                uint32_t* handler_offsets,
                                                uint32_t handlers_size) {
  CodeItemDataAccessor accessor(*dex_file_, code_item);
  const uint8_t* handlers_base = accessor.GetCatchHandlerData();

  for (uint32_t i = 0; i < handlers_size; i++) {
    bool catch_all;
    size_t offset = ptr_ - handlers_base;
    DECODE_SIGNED_CHECKED_FROM(ptr_, size);

    if (UNLIKELY((size < -65536) || (size > 65536))) {
      ErrorStringPrintf("Invalid exception handler size: %d", size);
      return false;
    }

    if (size <= 0) {
      catch_all = true;
      size = -size;
    } else {
      catch_all = false;
    }

    handler_offsets[i] = static_cast<uint32_t>(offset);

    while (size-- > 0) {
      DECODE_UNSIGNED_CHECKED_FROM(ptr_, type_idx);
      if (!CheckIndex(type_idx, header_->type_ids_size_, "handler type_idx")) {
        return false;
      }

      DECODE_UNSIGNED_CHECKED_FROM(ptr_, addr);
      if (UNLIKELY(addr >= accessor.InsnsSizeInCodeUnits())) {
        ErrorStringPrintf("Invalid handler addr: %x", addr);
        return false;
      }
    }

    if (catch_all) {
      DECODE_UNSIGNED_CHECKED_FROM(ptr_, addr);
      if (UNLIKELY(addr >= accessor.InsnsSizeInCodeUnits())) {
        ErrorStringPrintf("Invalid handler catch_all_addr: %x", addr);
        return false;
      }
    }
  }

  return true;
}

bool DexFileVerifier::CheckClassDataItemField(uint32_t idx,
                                              uint32_t access_flags,
                                              uint32_t class_access_flags,
                                              dex::TypeIndex class_type_index) {
  // The `idx` has already been checked in `CheckIntraClassDataItemFields()`.
  DCHECK_LE(idx, header_->field_ids_size_);

  // Check that it's the right class.
  dex::TypeIndex my_class_index =
      OffsetToPtr<dex::FieldId>(header_->field_ids_off_)[idx].class_idx_;
  if (class_type_index != my_class_index) {
    ErrorStringPrintf("Field's class index unexpected, %" PRIu16 "vs %" PRIu16,
                      my_class_index.index_,
                      class_type_index.index_);
    return false;
  }

  // Check field access flags.
  std::string error_msg;
  if (!CheckFieldAccessFlags(idx, access_flags, class_access_flags, &error_msg)) {
    ErrorStringPrintf("%s", error_msg.c_str());
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckClassDataItemMethod(uint32_t idx,
                                               uint32_t access_flags,
                                               uint32_t class_access_flags,
                                               dex::TypeIndex class_type_index,
                                               uint32_t code_offset,
                                               bool expect_direct) {
  // The `idx` has already been checked in `CheckIntraClassDataItemMethods()`.
  DCHECK_LT(idx, header_->method_ids_size_);

  const dex::MethodId& method_id = OffsetToPtr<dex::MethodId>(header_->method_ids_off_)[idx];

  // Check that it's the right class.
  dex::TypeIndex my_class_index = method_id.class_idx_;
  if (class_type_index != my_class_index) {
    ErrorStringPrintf("Method's class index unexpected, %" PRIu16 " vs %" PRIu16,
                      my_class_index.index_,
                      class_type_index.index_);
    return false;
  }

  std::string error_msg;
  uint32_t constructor_flags_by_name = 0;
  {
    uint32_t string_idx = method_id.name_idx_.index_;
    if (!CheckIndex(string_idx, header_->string_ids_size_, "method flags verification")) {
      return false;
    }
    if (UNLIKELY(string_idx < init_indices_.angle_bracket_end_index) &&
            string_idx >= init_indices_.angle_bracket_start_index) {
      if (string_idx == init_indices_.angle_clinit_angle_index) {
        constructor_flags_by_name = kAccStatic | kAccConstructor;
      } else if (string_idx == init_indices_.angle_init_angle_index) {
        constructor_flags_by_name = kAccConstructor;
      } else {
        ErrorStringPrintf("Bad method name for method index %u", idx);
        return false;
      }
    }
  }

  bool has_code = (code_offset != 0);
  if (!CheckMethodAccessFlags(idx,
                              access_flags,
                              class_access_flags,
                              constructor_flags_by_name,
                              has_code,
                              expect_direct,
                              &error_msg)) {
    ErrorStringPrintf("%s", error_msg.c_str());
    return false;
  }

  if (constructor_flags_by_name != 0) {
    if (!CheckConstructorProperties(idx, constructor_flags_by_name)) {
      DCHECK(FailureReasonIsSet());
      return false;
    }
  }

  return true;
}

bool DexFileVerifier::CheckPadding(uint32_t aligned_offset,
                                   DexFile::MapItemType type) {
  size_t offset = PtrToOffset(ptr_);
  if (offset < aligned_offset) {
    if (!CheckListSize(OffsetToPtr(offset), aligned_offset - offset, sizeof(uint8_t), "section")) {
      return false;
    }
    if (dex_version_ >= 41) {
      ptr_ += aligned_offset - offset;
      return true;
    }
    while (offset < aligned_offset) {
      if (UNLIKELY(*ptr_ != '\0')) {
        ErrorStringPrintf("Non-zero padding %x before section of type %zu at offset 0x%zx",
                          *ptr_,
                          static_cast<size_t>(type),
                          offset);
        return false;
      }
      ptr_++;
      offset++;
    }
  }
  return true;
}

bool DexFileVerifier::CheckEncodedValue() {
  if (!CheckListSize(ptr_, 1, sizeof(uint8_t), "encoded_value header")) {
    return false;
  }

  uint8_t header_byte = *(ptr_++);
  uint32_t value_type = header_byte & DexFile::kDexAnnotationValueTypeMask;
  uint32_t value_arg = header_byte >> DexFile::kDexAnnotationValueArgShift;

  switch (value_type) {
    case DexFile::kDexAnnotationByte:
      if (UNLIKELY(value_arg != 0)) {
        ErrorStringPrintf("Bad encoded_value byte size %x", value_arg);
        return false;
      }
      ptr_++;
      break;
    case DexFile::kDexAnnotationShort:
    case DexFile::kDexAnnotationChar:
      if (UNLIKELY(value_arg > 1)) {
        ErrorStringPrintf("Bad encoded_value char/short size %x", value_arg);
        return false;
      }
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationInt:
    case DexFile::kDexAnnotationFloat:
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value int/float size %x", value_arg);
        return false;
      }
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationLong:
    case DexFile::kDexAnnotationDouble:
      ptr_ += value_arg + 1;
      break;
    case DexFile::kDexAnnotationString: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value string size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->string_ids_size_, "encoded_value string")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationType: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value type size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->type_ids_size_, "encoded_value type")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationField:
    case DexFile::kDexAnnotationEnum: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value field/enum size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->field_ids_size_, "encoded_value field")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationMethod: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value method size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->method_ids_size_, "encoded_value method")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationArray:
      if (UNLIKELY(value_arg != 0)) {
        ErrorStringPrintf("Bad encoded_value array value_arg %x", value_arg);
        return false;
      }
      if (!CheckEncodedArray()) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationAnnotation:
      if (UNLIKELY(value_arg != 0)) {
        ErrorStringPrintf("Bad encoded_value annotation value_arg %x", value_arg);
        return false;
      }
      if (!CheckEncodedAnnotation()) {
        return false;
      }
      break;
    case DexFile::kDexAnnotationNull:
      if (UNLIKELY(value_arg != 0)) {
        ErrorStringPrintf("Bad encoded_value null value_arg %x", value_arg);
        return false;
      }
      break;
    case DexFile::kDexAnnotationBoolean:
      if (UNLIKELY(value_arg > 1)) {
        ErrorStringPrintf("Bad encoded_value boolean size %x", value_arg);
        return false;
      }
      break;
    case DexFile::kDexAnnotationMethodType: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value method type size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, header_->proto_ids_size_, "method_type value")) {
        return false;
      }
      break;
    }
    case DexFile::kDexAnnotationMethodHandle: {
      if (UNLIKELY(value_arg > 3)) {
        ErrorStringPrintf("Bad encoded_value method handle size %x", value_arg);
        return false;
      }
      uint32_t idx = ReadUnsignedLittleEndian(value_arg + 1);
      if (!CheckIndex(idx, dex_file_->NumMethodHandles(), "method_handle value")) {
        return false;
      }
      break;
    }
    default:
      ErrorStringPrintf("Bogus encoded_value value_type %x", value_type);
      return false;
  }

  return true;
}

bool DexFileVerifier::CheckEncodedArray() {
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, size);

  for (; size != 0u; --size) {
    if (!CheckEncodedValue()) {
      failure_reason_ = StringPrintf("Bad encoded_array value: %s", failure_reason_.c_str());
      return false;
    }
  }
  return true;
}

bool DexFileVerifier::CheckEncodedAnnotation() {
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, anno_idx);
  if (!CheckIndex(anno_idx, header_->type_ids_size_, "encoded_annotation type_idx")) {
    return false;
  }

  DECODE_UNSIGNED_CHECKED_FROM(ptr_, size);
  uint32_t last_idx = 0;

  for (uint32_t i = 0; i < size; i++) {
    DECODE_UNSIGNED_CHECKED_FROM(ptr_, idx);
    if (!CheckIndex(idx, header_->string_ids_size_, "annotation_element name_idx")) {
      return false;
    }

    if (UNLIKELY(last_idx >= idx && i != 0)) {
      ErrorStringPrintf("Out-of-order annotation_element name_idx: %x then %x",
                        last_idx, idx);
      return false;
    }

    if (!CheckEncodedValue()) {
      return false;
    }

    last_idx = idx;
  }
  return true;
}

bool DexFileVerifier::CheckStaticFieldTypes(const dex::ClassDef& class_def) {
  ClassAccessor accessor(*dex_file_, ptr_);
  EncodedStaticFieldValueIterator array_it(*dex_file_, class_def);

  for (const ClassAccessor::Field& field : accessor.GetStaticFields()) {
    if (!array_it.HasNext()) {
      break;
    }
    uint32_t index = field.GetIndex();
    // The `index` has already been checked in `CheckIntraClassDataItemFields()`.
    DCHECK_LT(index, header_->field_ids_size_);
    const dex::TypeId& type_id = dex_file_->GetTypeId(dex_file_->GetFieldId(index).type_idx_);
    const char* field_type_name =
        dex_file_->GetStringData(dex_file_->GetStringId(type_id.descriptor_idx_));
    Primitive::Type field_type = Primitive::GetType(field_type_name[0]);
    EncodedArrayValueIterator::ValueType array_type = array_it.GetValueType();
    // Ensure this matches RuntimeEncodedStaticFieldValueIterator.
    switch (array_type) {
      case EncodedArrayValueIterator::ValueType::kBoolean:
        if (field_type != Primitive::kPrimBoolean) {
          ErrorStringPrintf("unexpected static field initial value type: 'Z' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kByte:
        if (field_type != Primitive::kPrimByte) {
          ErrorStringPrintf("unexpected static field initial value type: 'B' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kShort:
        if (field_type != Primitive::kPrimShort) {
          ErrorStringPrintf("unexpected static field initial value type: 'S' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kChar:
        if (field_type != Primitive::kPrimChar) {
          ErrorStringPrintf("unexpected static field initial value type: 'C' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kInt:
        if (field_type != Primitive::kPrimInt) {
          ErrorStringPrintf("unexpected static field initial value type: 'I' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kLong:
        if (field_type != Primitive::kPrimLong) {
          ErrorStringPrintf("unexpected static field initial value type: 'J' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kFloat:
        if (field_type != Primitive::kPrimFloat) {
          ErrorStringPrintf("unexpected static field initial value type: 'F' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kDouble:
        if (field_type != Primitive::kPrimDouble) {
          ErrorStringPrintf("unexpected static field initial value type: 'D' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      case EncodedArrayValueIterator::ValueType::kNull:
      case EncodedArrayValueIterator::ValueType::kString:
      case EncodedArrayValueIterator::ValueType::kType:
        if (field_type != Primitive::kPrimNot) {
          ErrorStringPrintf("unexpected static field initial value type: 'L' vs '%c'",
                            field_type_name[0]);
          return false;
        }
        break;
      default:
        ErrorStringPrintf("unexpected static field initial value type: %x", array_type);
        return false;
    }
    if (!array_it.MaybeNext()) {
      ErrorStringPrintf("unexpected encoded value type: '%c'", array_it.GetValueType());
      return false;
    }
  }

  if (array_it.HasNext()) {
    ErrorStringPrintf("too many static field initial values");
    return false;
  }
  return true;
}

bool DexFileVerifier::CheckIntraTypeIdItem() {
  if (!CheckListSize(ptr_, 1, sizeof(dex::TypeId), "type_ids")) {
    return false;
  }

  const dex::TypeId* type_id = reinterpret_cast<const dex::TypeId*>(ptr_);
  if (!CheckIndex(type_id->descriptor_idx_.index_,
                  header_->string_ids_size_,
                  "type_id.descriptor")) {
    return false;
  }

  ptr_ += sizeof(dex::TypeId);
  return true;
}

bool DexFileVerifier::CheckIntraProtoIdItem() {
  if (!CheckListSize(ptr_, 1, sizeof(dex::ProtoId), "proto_ids")) {
    return false;
  }

  const dex::ProtoId* proto_id = reinterpret_cast<const dex::ProtoId*>(ptr_);
  if (!CheckIndex(proto_id->shorty_idx_.index_, header_->string_ids_size_, "proto_id.shorty") ||
      !CheckIndex(proto_id->return_type_idx_.index_,
                  header_->type_ids_size_,
                  "proto_id.return_type")) {
    return false;
  }

  ptr_ += sizeof(dex::ProtoId);
  return true;
}

bool DexFileVerifier::CheckIntraFieldIdItem() {
  if (!CheckListSize(ptr_, 1, sizeof(dex::FieldId), "field_ids")) {
    return false;
  }

  const dex::FieldId* field_id = reinterpret_cast<const dex::FieldId*>(ptr_);
  if (!CheckIndex(field_id->class_idx_.index_, header_->type_ids_size_, "field_id.class") ||
      !CheckIndex(field_id->type_idx_.index_, header_->type_ids_size_, "field_id.type") ||
      !CheckIndex(field_id->name_idx_.index_, header_->string_ids_size_, "field_id.name")) {
    return false;
  }

  ptr_ += sizeof(dex::FieldId);
  return true;
}

bool DexFileVerifier::CheckIntraMethodIdItem() {
  if (!CheckListSize(ptr_, 1, sizeof(dex::MethodId), "method_ids")) {
    return false;
  }

  const dex::MethodId* method_id = reinterpret_cast<const dex::MethodId*>(ptr_);
  if (!CheckIndex(method_id->class_idx_.index_, header_->type_ids_size_, "method_id.class") ||
      !CheckIndex(method_id->proto_idx_.index_, header_->proto_ids_size_, "method_id.proto") ||
      !CheckIndex(method_id->name_idx_.index_, header_->string_ids_size_, "method_id.name")) {
    return false;
  }

  ptr_ += sizeof(dex::MethodId);
  return true;
}

bool DexFileVerifier::CheckIntraClassDefItem(uint32_t class_def_index) {
  if (!CheckListSize(ptr_, 1, sizeof(dex::ClassDef), "class_defs")) {
    return false;
  }

  const dex::ClassDef* class_def = reinterpret_cast<const dex::ClassDef*>(ptr_);
  if (!CheckIndex(class_def->class_idx_.index_, header_->type_ids_size_, "class_def.class")) {
    return false;
  }

  // Check superclass, if any.
  if (UNLIKELY(class_def->pad2_ != 0u)) {
    uint32_t combined =
        (static_cast<uint32_t>(class_def->pad2_) << 16) + class_def->superclass_idx_.index_;
    if (combined != 0xffffffffu) {
      ErrorStringPrintf("Invalid superclass type padding/index: %x", combined);
      return false;
    }
  } else if (!CheckIndex(class_def->superclass_idx_.index_,
                         header_->type_ids_size_,
                         "class_def.superclass")) {
    return false;
  }

  DCHECK_LE(class_def->class_idx_.index_, kTypeIdLimit);
  DCHECK_LT(kTypeIdLimit, defined_classes_.size());
  if (defined_classes_[class_def->class_idx_.index_]) {
    ErrorStringPrintf("Redefinition of class with type idx: '%u'", class_def->class_idx_.index_);
    return false;
  }
  defined_classes_[class_def->class_idx_.index_] = true;
  DCHECK_LE(class_def->class_idx_.index_, defined_class_indexes_.size());
  defined_class_indexes_[class_def->class_idx_.index_] = class_def_index;

  ptr_ += sizeof(dex::ClassDef);
  return true;
}

bool DexFileVerifier::CheckIntraMethodHandleItem() {
  if (!CheckListSize(ptr_, 1, sizeof(dex::MethodHandleItem), "method_handles")) {
    return false;
  }

  const dex::MethodHandleItem* item = reinterpret_cast<const dex::MethodHandleItem*>(ptr_);

  DexFile::MethodHandleType method_handle_type =
      static_cast<DexFile::MethodHandleType>(item->method_handle_type_);
  if (method_handle_type > DexFile::MethodHandleType::kLast) {
    ErrorStringPrintf("Bad method handle type %x", item->method_handle_type_);
    return false;
  }

  uint32_t index = item->field_or_method_idx_;
  switch (method_handle_type) {
    case DexFile::MethodHandleType::kStaticPut:
    case DexFile::MethodHandleType::kStaticGet:
    case DexFile::MethodHandleType::kInstancePut:
    case DexFile::MethodHandleType::kInstanceGet:
      if (!CheckIndex(index, header_->field_ids_size_, "method_handle_item field_idx")) {
        return false;
      }
      break;
    case DexFile::MethodHandleType::kInvokeStatic:
    case DexFile::MethodHandleType::kInvokeInstance:
    case DexFile::MethodHandleType::kInvokeConstructor:
    case DexFile::MethodHandleType::kInvokeDirect:
    case DexFile::MethodHandleType::kInvokeInterface: {
      if (!CheckIndex(index, header_->method_ids_size_, "method_handle_item method_idx")) {
        return false;
      }
      break;
    }
  }

  ptr_ += sizeof(dex::MethodHandleItem);
  return true;
}

bool DexFileVerifier::CheckIntraTypeList() {
  const dex::TypeList* type_list = reinterpret_cast<const dex::TypeList*>(ptr_);
  if (!CheckList(sizeof(dex::TypeItem), "type_list", &ptr_)) {
    return false;
  }

  for (uint32_t i = 0, size = type_list->Size(); i != size; ++i) {
    if (!CheckIndex(type_list->GetTypeItem(i).type_idx_.index_,
                    header_->type_ids_size_,
                    "type_list.type")) {
      return false;
    }
  }

  return true;
}

template <bool kStatic>
bool DexFileVerifier::CheckIntraClassDataItemFields(size_t count) {
  constexpr const char* kTypeDescr = kStatic ? "static field" : "instance field";

  // We cannot use ClassAccessor::Field yet as it could read beyond the end of the data section.
  const uint8_t* ptr = ptr_;

  uint32_t prev_index = 0;
  for (size_t i = 0; i != count; ++i) {
    uint32_t field_idx_diff, access_flags;
    if (UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &field_idx_diff)) ||
        UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &access_flags))) {
      ErrorStringPrintf("encoded_field read out of bounds");
      return false;
    }
    uint32_t curr_index = prev_index + field_idx_diff;
    // Check for overflow.
    if (!CheckIndex(curr_index, header_->field_ids_size_, "class_data_item field_idx")) {
      return false;
    }
    if (!CheckOrder(kTypeDescr, curr_index, prev_index)) {
      return false;
    }
    // Check that it falls into the right class-data list.
    bool is_static = (access_flags & kAccStatic) != 0;
    if (UNLIKELY(is_static != kStatic)) {
      ErrorStringPrintf("Static/instance field not in expected list");
      return false;
    }

    prev_index = curr_index;
  }

  ptr_ = ptr;
  return true;
}

bool DexFileVerifier::CheckIntraClassDataItemMethods(size_t num_methods,
                                                     ClassAccessor::Method* direct_methods,
                                                     size_t num_direct_methods) {
  DCHECK(num_direct_methods == 0u || direct_methods != nullptr);
  const char* kTypeDescr = (direct_methods == nullptr) ? "direct method" : "virtual method";

  // We cannot use ClassAccessor::Method yet as it could read beyond the end of the data section.
  const uint8_t* ptr = ptr_;

  // Load the first direct method for the check below.
  size_t remaining_direct_methods = num_direct_methods;
  if (remaining_direct_methods != 0u) {
    DCHECK(direct_methods != nullptr);
    direct_methods->Read();
  }

  uint32_t prev_index = 0;
  for (size_t i = 0; i != num_methods; ++i) {
    uint32_t method_idx_diff, access_flags, code_off;
    if (UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &method_idx_diff)) ||
        UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &access_flags)) ||
        UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &code_off))) {
      ErrorStringPrintf("encoded_method read out of bounds");
      return false;
    }
    uint32_t curr_index = prev_index + method_idx_diff;
    // Check for overflow.
    if (!CheckIndex(curr_index, header_->method_ids_size_, "class_data_item method_idx")) {
      return false;
    }
    if (!CheckOrder(kTypeDescr, curr_index, prev_index)) {
      return false;
    }

    // For virtual methods, we cross reference the method index to make sure
    // it doesn't match any direct methods.
    if (remaining_direct_methods != 0) {
      // The direct methods are already known to be in ascending index order.
      // So just keep up with the current index.
      while (true) {
        const uint32_t direct_idx = direct_methods->GetIndex();
        if (direct_idx > curr_index) {
          break;
        }
        if (direct_idx == curr_index) {
          ErrorStringPrintf("Found virtual method with same index as direct method: %u",
                            curr_index);
          return false;
        }
        --remaining_direct_methods;
        if (remaining_direct_methods == 0u) {
          break;
        }
        direct_methods->Read();
      }
    }

    prev_index = curr_index;
  }

  ptr_ = ptr;
  return true;
}

bool DexFileVerifier::CheckIntraClassDataItem() {
  // We cannot use ClassAccessor yet as it could read beyond the end of the data section.
  const uint8_t* ptr = ptr_;
  uint32_t static_fields_size, instance_fields_size, direct_methods_size, virtual_methods_size;
  if (UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &static_fields_size)) ||
      UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &instance_fields_size)) ||
      UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &direct_methods_size)) ||
      UNLIKELY(!DecodeUnsignedLeb128Checked(&ptr, data_.end(), &virtual_methods_size))) {
    ErrorStringPrintf("class_data_item read out of bounds");
    return false;
  }
  ptr_ = ptr;

  // Check fields.
  if (!CheckIntraClassDataItemFields</*kStatic=*/ true>(static_fields_size)) {
    return false;
  }
  if (!CheckIntraClassDataItemFields</*kStatic=*/ false>(instance_fields_size)) {
    return false;
  }

  // Check methods.
  const uint8_t* direct_methods_ptr = ptr_;
  if (!CheckIntraClassDataItemMethods(direct_methods_size,
                                      /*direct_methods=*/ nullptr,
                                      /*num_direct_methods=*/ 0u)) {
    return false;
  }
  // Direct methods have been checked, so we can now use ClassAccessor::Method to read them again.
  ClassAccessor::Method direct_methods(*dex_file_, direct_methods_ptr);
  if (!CheckIntraClassDataItemMethods(virtual_methods_size, &direct_methods, direct_methods_size)) {
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraCodeItem() {
  const dex::CodeItem* code_item = reinterpret_cast<const dex::CodeItem*>(ptr_);

  DCHECK(dex_file_->IsStandardDexFile());
  if (!CheckListSize(code_item, 1, sizeof(StandardDexFile::CodeItem), "code")) {
    return false;
  }

  CodeItemDataAccessor accessor(*dex_file_, code_item);
  if (UNLIKELY(accessor.InsSize() > accessor.RegistersSize())) {
    ErrorStringPrintf("ins_size (%ud) > registers_size (%ud)",
                      accessor.InsSize(), accessor.RegistersSize());
    return false;
  }

  if (UNLIKELY(accessor.OutsSize() > 5 && accessor.OutsSize() > accessor.RegistersSize())) {
    /*
     * outs_size can be up to 5, even if registers_size is smaller, since the
     * short forms of method invocation allow repetitions of a register multiple
     * times within a single parameter list. However, longer parameter lists
     * need to be represented in-order in the register file.
     */
    ErrorStringPrintf("outs_size (%ud) > registers_size (%ud)",
                      accessor.OutsSize(), accessor.RegistersSize());
    return false;
  }

  const uint16_t* insns = accessor.Insns();
  uint32_t insns_size = accessor.InsnsSizeInCodeUnits();
  if (!CheckListSize(insns, insns_size, sizeof(uint16_t), "insns size")) {
    return false;
  }

  // Grab the end of the insns if there are no try_items.
  uint32_t try_items_size = accessor.TriesSize();
  if (try_items_size == 0) {
    ptr_ = reinterpret_cast<const uint8_t*>(&insns[insns_size]);
    return true;
  }

  const dex::TryItem* try_items = accessor.TryItems().begin();
  if (!CheckListSize(try_items, try_items_size, sizeof(dex::TryItem), "try_items size")) {
    return false;
  }

  // try_items are 4-byte aligned. Verify the spacer is 0.
  if (((reinterpret_cast<uintptr_t>(&insns[insns_size]) & 3) != 0) && (insns[insns_size] != 0)) {
    ErrorStringPrintf("Non-zero padding: %x", insns[insns_size]);
    return false;
  }

  ptr_ = accessor.GetCatchHandlerData();
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, handlers_size);

  if (UNLIKELY((handlers_size == 0) || (handlers_size >= 65536))) {
    ErrorStringPrintf("Invalid handlers_size: %ud", handlers_size);
    return false;
  }

  // Avoid an expensive allocation, if possible.
  std::unique_ptr<uint32_t[]> handler_offsets_uptr;
  uint32_t* handler_offsets;
  constexpr size_t kAllocaMaxSize = 1024;
  if (handlers_size < kAllocaMaxSize/sizeof(uint32_t)) {
    // Note: Clang does not specify alignment guarantees for alloca. So align by hand.
    handler_offsets =
        AlignUp(reinterpret_cast<uint32_t*>(alloca((handlers_size + 1) * sizeof(uint32_t))),
                alignof(uint32_t[]));
  } else {
    handler_offsets_uptr.reset(new uint32_t[handlers_size]);
    handler_offsets = handler_offsets_uptr.get();
  }

  if (!CheckAndGetHandlerOffsets(code_item, &handler_offsets[0], handlers_size)) {
    return false;
  }

  uint32_t last_addr = 0;
  for (; try_items_size != 0u; --try_items_size) {
    if (UNLIKELY(try_items->start_addr_ < last_addr)) {
      ErrorStringPrintf("Out-of_order try_item with start_addr: %x", try_items->start_addr_);
      return false;
    }

    if (UNLIKELY(try_items->start_addr_ >= insns_size)) {
      ErrorStringPrintf("Invalid try_item start_addr: %x", try_items->start_addr_);
      return false;
    }

    uint32_t i;
    for (i = 0; i < handlers_size; i++) {
      if (try_items->handler_off_ == handler_offsets[i]) {
        break;
      }
    }

    if (UNLIKELY(i == handlers_size)) {
      ErrorStringPrintf("Bogus handler offset: %x", try_items->handler_off_);
      return false;
    }

    last_addr = try_items->start_addr_ + try_items->insn_count_;
    if (UNLIKELY(last_addr > insns_size)) {
      ErrorStringPrintf("Invalid try_item insn_count: %x", try_items->insn_count_);
      return false;
    }

    try_items++;
  }

  return true;
}

bool DexFileVerifier::CheckIntraStringDataItem() {
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, size);
  const uint8_t* file_end = EndOfFile();

  for (uint32_t i = 0; i < size; i++) {
    CHECK_LT(i, size);  // b/15014252 Prevents hitting the impossible case below
    if (UNLIKELY(ptr_ >= file_end)) {
      ErrorStringPrintf("String data would go beyond end-of-file");
      return false;
    }

    uint8_t byte = *(ptr_++);

    // Switch on the high 4 bits.
    switch (byte >> 4) {
      case 0x00:
        // Special case of bit pattern 0xxx.
        if (UNLIKELY(byte == 0)) {
          CHECK_LT(i, size);  // b/15014252 Actually hit this impossible case with clang
          ErrorStringPrintf("String data shorter than indicated utf16_size %x", size);
          return false;
        }
        break;
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
        // No extra checks necessary for bit pattern 0xxx.
        break;
      case 0x08:
      case 0x09:
      case 0x0a:
      case 0x0b:
      case 0x0f:
        // Illegal bit patterns 10xx or 1111.
        // Note: 1111 is valid for normal UTF-8, but not here.
        ErrorStringPrintf("Illegal start byte %x in string data", byte);
        return false;
      case 0x0c:
      case 0x0d: {
        // Bit pattern 110x has an additional byte.
        uint8_t byte2 = *(ptr_++);
        if (UNLIKELY((byte2 & 0xc0) != 0x80)) {
          ErrorStringPrintf("Illegal continuation byte %x in string data", byte2);
          return false;
        }
        uint16_t value = ((byte & 0x1f) << 6) | (byte2 & 0x3f);
        if (UNLIKELY((value != 0) && (value < 0x80))) {
          ErrorStringPrintf("Illegal representation for value %x in string data", value);
          return false;
        }
        break;
      }
      case 0x0e: {
        // Bit pattern 1110 has 2 additional bytes.
        uint8_t byte2 = *(ptr_++);
        if (UNLIKELY((byte2 & 0xc0) != 0x80)) {
          ErrorStringPrintf("Illegal continuation byte %x in string data", byte2);
          return false;
        }
        uint8_t byte3 = *(ptr_++);
        if (UNLIKELY((byte3 & 0xc0) != 0x80)) {
          ErrorStringPrintf("Illegal continuation byte %x in string data", byte3);
          return false;
        }
        uint16_t value = ((byte & 0x0f) << 12) | ((byte2 & 0x3f) << 6) | (byte3 & 0x3f);
        if (UNLIKELY(value < 0x800)) {
          ErrorStringPrintf("Illegal representation for value %x in string data", value);
          return false;
        }
        break;
      }
    }
  }

  if (UNLIKELY(*(ptr_++) != '\0')) {
    ErrorStringPrintf("String longer than indicated size %x", size);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraDebugInfoItem() {
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, unused_line_start);
  DECODE_UNSIGNED_CHECKED_FROM(ptr_, parameters_size);
  if (UNLIKELY(parameters_size > 65536)) {
    ErrorStringPrintf("Invalid parameters_size: %x", parameters_size);
    return false;
  }

  for (uint32_t j = 0; j < parameters_size; j++) {
    DECODE_UNSIGNED_CHECKED_FROM(ptr_, parameter_name);
    if (parameter_name != 0) {
      parameter_name--;
      if (!CheckIndex(parameter_name, header_->string_ids_size_, "debug_info_item parameter_name")) {
        return false;
      }
    }
  }

  while (true) {
    if (UNLIKELY(ptr_ >= EndOfFile())) {
      // Went past the end.
      return false;
    }
    uint8_t opcode = *(ptr_++);
    switch (opcode) {
      case DexFile::DBG_END_SEQUENCE: {
        return true;
      }
      case DexFile::DBG_ADVANCE_PC: {
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, unused_advance_pc);
        break;
      }
      case DexFile::DBG_ADVANCE_LINE: {
        DECODE_SIGNED_CHECKED_FROM(ptr_, unused_advance_line);
        break;
      }
      case DexFile::DBG_START_LOCAL: {
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, reg_num);
        if (UNLIKELY(reg_num >= 65536)) {
          ErrorStringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, name_idx);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_START_LOCAL name_idx")) {
            return false;
          }
        }
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, type_idx);
        if (type_idx != 0) {
          type_idx--;
          if (!CheckIndex(type_idx, header_->type_ids_size_, "DBG_START_LOCAL type_idx")) {
            return false;
          }
        }
        break;
      }
      case DexFile::DBG_END_LOCAL:
      case DexFile::DBG_RESTART_LOCAL: {
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, reg_num);
        if (UNLIKELY(reg_num >= 65536)) {
          ErrorStringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        break;
      }
      case DexFile::DBG_START_LOCAL_EXTENDED: {
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, reg_num);
        if (UNLIKELY(reg_num >= 65536)) {
          ErrorStringPrintf("Bad reg_num for opcode %x", opcode);
          return false;
        }
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, name_idx);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_START_LOCAL_EXTENDED name_idx")) {
            return false;
          }
        }
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, type_idx);
        if (type_idx != 0) {
          type_idx--;
          if (!CheckIndex(type_idx, header_->type_ids_size_, "DBG_START_LOCAL_EXTENDED type_idx")) {
            return false;
          }
        }
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, sig_idx);
        if (sig_idx != 0) {
          sig_idx--;
          if (!CheckIndex(sig_idx, header_->string_ids_size_, "DBG_START_LOCAL_EXTENDED sig_idx")) {
            return false;
          }
        }
        break;
      }
      case DexFile::DBG_SET_FILE: {
        DECODE_UNSIGNED_CHECKED_FROM(ptr_, name_idx);
        if (name_idx != 0) {
          name_idx--;
          if (!CheckIndex(name_idx, header_->string_ids_size_, "DBG_SET_FILE name_idx")) {
            return false;
          }
        }
        break;
      }
    }
  }
}

bool DexFileVerifier::CheckIntraAnnotationItem() {
  if (!CheckListSize(ptr_, 1, sizeof(uint8_t), "annotation visibility")) {
    return false;
  }

  // Check visibility
  switch (*(ptr_++)) {
    case DexFile::kDexVisibilityBuild:
    case DexFile::kDexVisibilityRuntime:
    case DexFile::kDexVisibilitySystem:
      break;
    default:
      ErrorStringPrintf("Bad annotation visibility: %x", *ptr_);
      return false;
  }

  if (!CheckEncodedAnnotation()) {
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraHiddenapiClassData() {
  const dex::HiddenapiClassData* item = reinterpret_cast<const dex::HiddenapiClassData*>(ptr_);

  // Check expected header size.
  uint32_t num_header_elems = dex_file_->NumClassDefs() + 1;
  uint32_t elem_size = sizeof(uint32_t);
  uint32_t header_size = num_header_elems * elem_size;
  if (!CheckListSize(item, num_header_elems, elem_size, "hiddenapi class data section header")) {
    return false;
  }

  // Check total size.
  if (!CheckListSize(item, item->size_, 1u, "hiddenapi class data section")) {
    return false;
  }

  // Check that total size can fit header.
  if (item->size_ < header_size) {
    ErrorStringPrintf(
        "Hiddenapi class data too short to store header (%u < %u)", item->size_, header_size);
    return false;
  }

  // The rest of the section depends on the class_data_item being verified first. We will finalize
  // verifying the hiddenapi_class_data_item in CheckInterHiddenapiClassData.
  const uint8_t* data_end = ptr_ + item->size_;
  ptr_ = data_end;
  return true;
}

bool DexFileVerifier::CheckIntraAnnotationsDirectoryItem() {
  const dex::AnnotationsDirectoryItem* item =
      reinterpret_cast<const dex::AnnotationsDirectoryItem*>(ptr_);
  if (!CheckListSize(item, 1, sizeof(dex::AnnotationsDirectoryItem), "annotations_directory")) {
    return false;
  }

  // Field annotations follow immediately after the annotations directory.
  const dex::FieldAnnotationsItem* field_item =
      reinterpret_cast<const dex::FieldAnnotationsItem*>(item + 1);
  uint32_t field_count = item->fields_size_;
  if (!CheckListSize(field_item,
                     field_count,
                     sizeof(dex::FieldAnnotationsItem),
                     "field_annotations list")) {
    return false;
  }

  uint32_t last_idx = 0;
  for (uint32_t i = 0; i < field_count; i++) {
    if (!CheckIndex(field_item->field_idx_, header_->field_ids_size_, "field annotation")) {
      return false;
    }
    if (UNLIKELY(last_idx >= field_item->field_idx_ && i != 0)) {
      ErrorStringPrintf("Out-of-order field_idx for annotation: %x then %x",
                        last_idx, field_item->field_idx_);
      return false;
    }
    last_idx = field_item->field_idx_;
    field_item++;
  }

  // Method annotations follow immediately after field annotations.
  const dex::MethodAnnotationsItem* method_item =
      reinterpret_cast<const dex::MethodAnnotationsItem*>(field_item);
  uint32_t method_count = item->methods_size_;
  if (!CheckListSize(method_item,
                     method_count,
                     sizeof(dex::MethodAnnotationsItem),
                     "method_annotations list")) {
    return false;
  }

  last_idx = 0;
  for (uint32_t i = 0; i < method_count; i++) {
    if (!CheckIndex(method_item->method_idx_, header_->method_ids_size_, "method annotation")) {
      return false;
    }
    if (UNLIKELY(last_idx >= method_item->method_idx_ && i != 0)) {
      ErrorStringPrintf("Out-of-order method_idx for annotation: %x then %x",
                       last_idx, method_item->method_idx_);
      return false;
    }
    last_idx = method_item->method_idx_;
    method_item++;
  }

  // Parameter annotations follow immediately after method annotations.
  const dex::ParameterAnnotationsItem* parameter_item =
      reinterpret_cast<const dex::ParameterAnnotationsItem*>(method_item);
  uint32_t parameter_count = item->parameters_size_;
  if (!CheckListSize(parameter_item, parameter_count, sizeof(dex::ParameterAnnotationsItem),
                     "parameter_annotations list")) {
    return false;
  }

  last_idx = 0;
  for (uint32_t i = 0; i < parameter_count; i++) {
    if (!CheckIndex(parameter_item->method_idx_,
                    header_->method_ids_size_,
                    "parameter annotation method")) {
      return false;
    }
    if (UNLIKELY(last_idx >= parameter_item->method_idx_ && i != 0)) {
      ErrorStringPrintf("Out-of-order method_idx for annotation: %x then %x",
                        last_idx, parameter_item->method_idx_);
      return false;
    }
    last_idx = parameter_item->method_idx_;
    parameter_item++;
  }

  // Return a pointer to the end of the annotations.
  ptr_ = reinterpret_cast<const uint8_t*>(parameter_item);
  return true;
}

template <DexFile::MapItemType kType>
bool DexFileVerifier::CheckIntraSectionIterate(uint32_t section_count) {
  // Get the right alignment mask for the type of section.
  size_t alignment_mask;
  switch (kType) {
    case DexFile::kDexTypeClassDataItem:
    case DexFile::kDexTypeStringDataItem:
    case DexFile::kDexTypeDebugInfoItem:
    case DexFile::kDexTypeAnnotationItem:
    case DexFile::kDexTypeEncodedArrayItem:
      alignment_mask = sizeof(uint8_t) - 1;
      break;
    default:
      alignment_mask = sizeof(uint32_t) - 1;
      break;
  }

  // Iterate through the items in the section.
  for (uint32_t i = 0; i < section_count; i++) {
    size_t aligned_offset = (PtrToOffset(ptr_) + alignment_mask) & ~alignment_mask;

    // Check the padding between items.
    if (!CheckPadding(aligned_offset, kType)) {
      return false;
    }

    // Check depending on the section type.
    const uint8_t* start_ptr = ptr_;
    switch (kType) {
      case DexFile::kDexTypeStringIdItem: {
        if (!CheckListSize(ptr_, 1, sizeof(dex::StringId), "string_ids")) {
          return false;
        }
        ptr_ += sizeof(dex::StringId);
        break;
      }
      case DexFile::kDexTypeTypeIdItem: {
        if (!CheckIntraTypeIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeProtoIdItem: {
        if (!CheckIntraProtoIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeFieldIdItem: {
        if (!CheckIntraFieldIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeMethodIdItem: {
        if (!CheckIntraMethodIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDefItem: {
        if (!CheckIntraClassDefItem(/*class_def_index=*/ i)) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeCallSiteIdItem: {
        if (!CheckListSize(ptr_, 1, sizeof(dex::CallSiteIdItem), "call_site_ids")) {
          return false;
        }
        ptr_ += sizeof(dex::CallSiteIdItem);
        break;
      }
      case DexFile::kDexTypeMethodHandleItem: {
        if (!CheckIntraMethodHandleItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeTypeList: {
        if (!CheckIntraTypeList()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetRefList: {
        if (!CheckList(sizeof(dex::AnnotationSetRefItem), "annotation_set_ref_list", &ptr_)) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetItem: {
        if (!CheckList(sizeof(uint32_t), "annotation_set_item", &ptr_)) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDataItem: {
        if (!CheckIntraClassDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeCodeItem: {
        if (!CheckIntraCodeItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeStringDataItem: {
        if (!CheckIntraStringDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeDebugInfoItem: {
        if (!CheckIntraDebugInfoItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationItem: {
        if (!CheckIntraAnnotationItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeEncodedArrayItem: {
        if (!CheckEncodedArray()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationsDirectoryItem: {
        if (!CheckIntraAnnotationsDirectoryItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeHiddenapiClassData: {
        if (!CheckIntraHiddenapiClassData()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeHeaderItem:
      case DexFile::kDexTypeMapList:
        break;
    }

    if (start_ptr == ptr_) {
      ErrorStringPrintf("Unknown map item type %x", kType);
      return false;
    }

    if (IsDataSectionType(kType)) {
      if (aligned_offset == 0u) {
        ErrorStringPrintf("Item %d offset is 0", i);
        return false;
      }
      DCHECK(offset_to_type_map_.find(aligned_offset) == offset_to_type_map_.end());
      offset_to_type_map_.insert(std::pair<uint32_t, uint16_t>(aligned_offset, kType));
    }

    if (!PtrToOffset(ptr_, &aligned_offset)) {
      ErrorStringPrintf("Item %d at ends out of bounds", i);
      return false;
    }
  }

  return true;
}

template <DexFile::MapItemType kType>
bool DexFileVerifier::CheckIntraIdSection(size_t offset, uint32_t count) {
  uint32_t expected_offset;
  uint32_t expected_size;

  // Get the expected offset and size from the header.
  switch (kType) {
    case DexFile::kDexTypeStringIdItem:
      expected_offset = header_->string_ids_off_;
      expected_size = header_->string_ids_size_;
      break;
    case DexFile::kDexTypeTypeIdItem:
      expected_offset = header_->type_ids_off_;
      expected_size = header_->type_ids_size_;
      break;
    case DexFile::kDexTypeProtoIdItem:
      expected_offset = header_->proto_ids_off_;
      expected_size = header_->proto_ids_size_;
      break;
    case DexFile::kDexTypeFieldIdItem:
      expected_offset = header_->field_ids_off_;
      expected_size = header_->field_ids_size_;
      break;
    case DexFile::kDexTypeMethodIdItem:
      expected_offset = header_->method_ids_off_;
      expected_size = header_->method_ids_size_;
      break;
    case DexFile::kDexTypeClassDefItem:
      expected_offset = header_->class_defs_off_;
      expected_size = header_->class_defs_size_;
      break;
    default:
      ErrorStringPrintf("Bad type for id section: %x", kType);
      return false;
  }

  // Check that the offset and size are what were expected from the header.
  if (UNLIKELY(offset != expected_offset)) {
    ErrorStringPrintf("Bad offset for section: got %zx, expected %x", offset, expected_offset);
    return false;
  }
  if (UNLIKELY(count != expected_size)) {
    ErrorStringPrintf("Bad size for section: got %x, expected %x", count, expected_size);
    return false;
  }

  return CheckIntraSectionIterate<kType>(count);
}

template <DexFile::MapItemType kType>
bool DexFileVerifier::CheckIntraDataSection(size_t offset, uint32_t count) {
  size_t data_start = PtrToOffset(data_.begin());
  size_t data_end = PtrToOffset(data_.end());

  // Check the validity of the offset of the section.
  if (UNLIKELY((offset < data_start) || (offset > data_end))) {
    ErrorStringPrintf("Bad offset for data subsection: %zx", offset);
    return false;
  }

  if (!CheckIntraSectionIterate<kType>(count)) {
    return false;
  }

  // FIXME: Doing this check late means we may have already read memory outside the
  // data section and potentially outside the file, thus risking a segmentation fault.
  size_t next_offset;
  if (!PtrToOffset(ptr_, &next_offset) || next_offset > data_end) {
    ErrorStringPrintf("Out-of-bounds end of data subsection: %zu data_off=%u data_size=%u",
                      next_offset,
                      header_->data_off_,
                      header_->data_size_);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckIntraSection() {
  const dex::MapList* map = OffsetToPtr<dex::MapList>(header_->map_off_);
  const dex::MapItem* item = map->list_;
  uint32_t count = map->size_;
  ptr_ = dex_file_->Begin();

  // Preallocate offset map to avoid some allocations. We can only guess from the list items,
  // not derived things.
  offset_to_type_map_.reserve(
      std::min(header_->class_defs_size_, 65535u) +
      std::min(header_->string_ids_size_, 65535u) +
      2 * std::min(header_->method_ids_size_, 65535u));

  // Check the items listed in the map.
  for (; count != 0u; --count) {
    const uint8_t* initial_ptr = ptr_;
    uint32_t section_offset = item->offset_;
    uint32_t section_count = item->size_;
    DexFile::MapItemType type = static_cast<DexFile::MapItemType>(item->type_);

    // Check for padding and overlap between items.
    size_t offset = PtrToOffset(ptr_);
    if (UNLIKELY(offset > section_offset)) {
      ErrorStringPrintf("Section overlap or out-of-order map: %zx, %x", offset, section_offset);
      return false;
    }
    if (!CheckPadding(section_offset, type)) {
      return false;
    }

    // Check each item based on its type.
    switch (type) {
      case DexFile::kDexTypeHeaderItem: {
        if (UNLIKELY(section_count != 1)) {
          ErrorStringPrintf("Multiple header items");
          return false;
        }
        uint32_t expected = dex_version_ >= 41 ? PtrToOffset(dex_file_->Begin()) : 0;
        if (UNLIKELY(section_offset != expected)) {
          ErrorStringPrintf("Header at %x, expected %x", section_offset, expected);
          return false;
        }
        ptr_ += header_->header_size_;
        break;
      }

#define CHECK_INTRA_ID_SECTION_CASE(type)                                   \
      case type:                                                            \
        if (!CheckIntraIdSection<type>(section_offset, section_count)) {    \
          return false;                                                     \
        }                                                                   \
        break;
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeStringIdItem)
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeTypeIdItem)
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeProtoIdItem)
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeFieldIdItem)
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeMethodIdItem)
      CHECK_INTRA_ID_SECTION_CASE(DexFile::kDexTypeClassDefItem)
#undef CHECK_INTRA_ID_SECTION_CASE

      case DexFile::kDexTypeMapList:
        if (UNLIKELY(section_count != 1)) {
          ErrorStringPrintf("Multiple map list items");
          return false;
        }
        if (UNLIKELY(section_offset != header_->map_off_)) {
          ErrorStringPrintf("Map not at header-defined offset: %x, expected %x",
                            section_offset, header_->map_off_);
          return false;
        }
        ptr_ += sizeof(uint32_t) + (map->size_ * sizeof(dex::MapItem));
        break;

#define CHECK_INTRA_SECTION_ITERATE_CASE(type)                                 \
      case type:                                                               \
        if (!CheckIntraSectionIterate<type>(section_count)) {  \
          return false;                                                        \
        }                                                                      \
        break;
      CHECK_INTRA_SECTION_ITERATE_CASE(DexFile::kDexTypeMethodHandleItem)
      CHECK_INTRA_SECTION_ITERATE_CASE(DexFile::kDexTypeCallSiteIdItem)
#undef CHECK_INTRA_SECTION_ITERATE_CASE

#define CHECK_INTRA_DATA_SECTION_CASE(type)                                 \
      case type:                                                            \
        if (!CheckIntraDataSection<type>(section_offset, section_count)) {  \
          return false;                                                     \
        }                                                                   \
        break;
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeTypeList)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeAnnotationSetRefList)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeAnnotationSetItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeClassDataItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeCodeItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeStringDataItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeDebugInfoItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeAnnotationItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeEncodedArrayItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeAnnotationsDirectoryItem)
      CHECK_INTRA_DATA_SECTION_CASE(DexFile::kDexTypeHiddenapiClassData)
#undef CHECK_INTRA_DATA_SECTION_CASE
    }

    if (ptr_ == initial_ptr) {
      ErrorStringPrintf("Unknown map item type %x", type);
      return false;
    }

    item++;
  }

  return true;
}

bool DexFileVerifier::CheckOffsetToTypeMap(size_t offset, uint16_t type) {
  DCHECK(offset_to_type_map_.find(0) == offset_to_type_map_.end());
  auto it = offset_to_type_map_.find(offset);
  if (UNLIKELY(it == offset_to_type_map_.end())) {
    ErrorStringPrintf("No data map entry found @ %zx; expected %x", offset, type);
    return false;
  }
  if (UNLIKELY(it->second != type)) {
    ErrorStringPrintf("Unexpected data map entry @ %zx; expected %x, found %x",
                      offset, type, it->second);
    return false;
  }
  return true;
}

uint32_t DexFileVerifier::FindFirstClassDataDefiner(const ClassAccessor& accessor) {
  // The data item and field/method indexes have already been checked in
  // `CheckIntraClassDataItem()` or its helper functions.
  if (accessor.NumFields() != 0) {
    ClassAccessor::Field read_field(*dex_file_, accessor.ptr_pos_);
    read_field.Read();
    DCHECK_LE(read_field.GetIndex(), dex_file_->NumFieldIds());
    return dex_file_->GetFieldId(read_field.GetIndex()).class_idx_.index_;
  }

  if (accessor.NumMethods() != 0) {
    ClassAccessor::Method read_method(*dex_file_, accessor.ptr_pos_);
    read_method.Read();
    DCHECK_LE(read_method.GetIndex(), dex_file_->NumMethodIds());
    return dex_file_->GetMethodId(read_method.GetIndex()).class_idx_.index_;
  }

  return kDexNoIndex;
}

uint32_t DexFileVerifier::FindFirstAnnotationsDirectoryDefiner(const uint8_t* ptr) {
  // The annotations directory and field/method indexes have already been checked in
  // `CheckIntraAnnotationsDirectoryItem()`.
  const dex::AnnotationsDirectoryItem* item =
      reinterpret_cast<const dex::AnnotationsDirectoryItem*>(ptr);

  if (item->fields_size_ != 0) {
    dex::FieldAnnotationsItem* field_items = (dex::FieldAnnotationsItem*) (item + 1);
    DCHECK_LE(field_items[0].field_idx_, dex_file_->NumFieldIds());
    return dex_file_->GetFieldId(field_items[0].field_idx_).class_idx_.index_;
  }

  if (item->methods_size_ != 0) {
    dex::MethodAnnotationsItem* method_items = (dex::MethodAnnotationsItem*) (item + 1);
    DCHECK_LE(method_items[0].method_idx_, dex_file_->NumMethodIds());
    return dex_file_->GetMethodId(method_items[0].method_idx_).class_idx_.index_;
  }

  if (item->parameters_size_ != 0) {
    dex::ParameterAnnotationsItem* parameter_items = (dex::ParameterAnnotationsItem*) (item + 1);
    DCHECK_LE(parameter_items[0].method_idx_, dex_file_->NumMethodIds());
    return dex_file_->GetMethodId(parameter_items[0].method_idx_).class_idx_.index_;
  }

  return kDexNoIndex;
}

bool DexFileVerifier::CheckInterHiddenapiClassData() {
  const dex::HiddenapiClassData* item = reinterpret_cast<const dex::HiddenapiClassData*>(ptr_);

  // Move pointer after the header. This data has been verified in CheckIntraHiddenapiClassData.
  uint32_t num_header_elems = dex_file_->NumClassDefs() + 1;
  uint32_t elem_size = sizeof(uint32_t);
  uint32_t header_size = num_header_elems * elem_size;
  const uint8_t* data_end = ptr_ + item->size_;
  ptr_ += header_size;

  // Check offsets for each class def.
  for (uint32_t i = 0; i < dex_file_->NumClassDefs(); ++i) {
    const dex::ClassDef& class_def = dex_file_->GetClassDef(i);
    const uint8_t* class_data = dex_file_->GetClassData(class_def);
    uint32_t offset = item->flags_offset_[i];

    if (offset == 0) {
      continue;
    }

    // Check that class defs with no class data do not have any hiddenapi class data.
    if (class_data == nullptr) {
      ErrorStringPrintf(
          "Hiddenapi class data offset not zero for class def %u with no class data", i);
      return false;
    }

    // Check that the offset is within the section.
    if (offset > item->size_) {
      ErrorStringPrintf(
          "Hiddenapi class data offset out of section bounds (%u > %u) for class def %u",
          offset, item->size_, i);
      return false;
    }

    // Check that the offset matches current pointer position. We do not allow
    // offsets into already parsed data, or gaps between class def data.
    uint32_t ptr_offset = ptr_ - reinterpret_cast<const uint8_t*>(item);
    if (offset != ptr_offset) {
      ErrorStringPrintf(
          "Hiddenapi class data unexpected offset (%u != %u) for class def %u",
          offset, ptr_offset, i);
      return false;
    }

    // Parse a uleb128 value for each field and method of this class.
    bool failure = false;
    auto fn_member = [&](const ClassAccessor::BaseItem& member, const char* member_type) {
      if (failure) {
        return;
      }
      uint32_t decoded_flags;
      if (!DecodeUnsignedLeb128Checked(&ptr_, data_end, &decoded_flags)) {
        ErrorStringPrintf("Hiddenapi class data value out of bounds (%p > %p) for %s %i",
                          ptr_, data_end, member_type, member.GetIndex());
        failure = true;
        return;
      }
      if (!hiddenapi::ApiList(decoded_flags).IsValid()) {
        ErrorStringPrintf("Hiddenapi class data flags invalid (%u) for %s %i",
                          decoded_flags, member_type, member.GetIndex());
        failure = true;
        return;
      }
    };
    auto fn_field = [&](const ClassAccessor::Field& field) { fn_member(field, "field"); };
    auto fn_method = [&](const ClassAccessor::Method& method) { fn_member(method, "method"); };
    ClassAccessor accessor(*dex_file_, class_data);
    accessor.VisitFieldsAndMethods(fn_field, fn_field, fn_method, fn_method);
    if (failure) {
      return false;
    }
  }

  if (ptr_ != data_end) {
    ErrorStringPrintf("Hiddenapi class data wrong reported size (%u != %u)",
                       static_cast<uint32_t>(ptr_ - reinterpret_cast<const uint8_t*>(item)),
                       item->size_);
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckInterStringIdItem() {
  const dex::StringId* item = reinterpret_cast<const dex::StringId*>(ptr_);

  // Note: The mapping to string data items is eagerly verified at the start of CheckInterSection().

  // Check ordering between items.
  if (previous_item_ != nullptr) {
    const dex::StringId* prev_item = reinterpret_cast<const dex::StringId*>(previous_item_);
    const char* prev_str = dex_file_->GetStringData(*prev_item);
    const char* str = dex_file_->GetStringData(*item);
    if (UNLIKELY(CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(prev_str, str) >= 0)) {
      ErrorStringPrintf("Out-of-order string_ids: '%s' then '%s'", prev_str, str);
      return false;
    }
  }

  ptr_ += sizeof(dex::StringId);
  return true;
}

bool DexFileVerifier::CheckInterTypeIdItem() {
  const dex::TypeId* item = reinterpret_cast<const dex::TypeId*>(ptr_);

  {
    // Translate to index to potentially use cache.
    // The check in `CheckIntraIdSection()` guarantees that this index is valid.
    size_t index = item - OffsetToPtr<dex::TypeId>(header_->type_ids_off_);
    DCHECK_LE(index, header_->type_ids_size_);
    if (UNLIKELY(!VerifyTypeDescriptor(
        dex::TypeIndex(static_cast<decltype(dex::TypeIndex::index_)>(index)),
        "Invalid type descriptor",
        [](char) { return true; }))) {
      return false;
    }
  }

  // Check ordering between items.
  if (previous_item_ != nullptr) {
    const dex::TypeId* prev_item = reinterpret_cast<const dex::TypeId*>(previous_item_);
    if (UNLIKELY(prev_item->descriptor_idx_ >= item->descriptor_idx_)) {
      ErrorStringPrintf("Out-of-order type_ids: %x then %x",
                        prev_item->descriptor_idx_.index_,
                        item->descriptor_idx_.index_);
      return false;
    }
  }

  ptr_ += sizeof(dex::TypeId);
  return true;
}

bool DexFileVerifier::CheckInterProtoIdItem() {
  const dex::ProtoId* item = reinterpret_cast<const dex::ProtoId*>(ptr_);

  const char* shorty = dex_file_->StringDataByIdx(item->shorty_idx_);

  if (item->parameters_off_ != 0 &&
      !CheckOffsetToTypeMap(item->parameters_off_, DexFile::kDexTypeTypeList)) {
    return false;
  }

  // Check that return type is representable as a uint16_t;
  if (UNLIKELY(!IsValidOrNoTypeId(item->return_type_idx_.index_, item->pad_))) {
    ErrorStringPrintf("proto with return type idx outside uint16_t range '%x:%x'",
                      item->pad_, item->return_type_idx_.index_);
    return false;
  }
  // Check the return type and advance the shorty.
  const char* return_type = dex_file_->StringByTypeIdx(item->return_type_idx_);
  if (!CheckShortyDescriptorMatch(*shorty, return_type, true)) {
    return false;
  }
  shorty++;

  DexFileParameterIterator it(*dex_file_, *item);
  while (it.HasNext() && *shorty != '\0') {
    if (!CheckIndex(it.GetTypeIdx().index_,
                    dex_file_->NumTypeIds(),
                    "inter_proto_id_item shorty type_idx")) {
      return false;
    }
    const char* descriptor = it.GetDescriptor();
    if (!CheckShortyDescriptorMatch(*shorty, descriptor, false)) {
      return false;
    }
    it.Next();
    shorty++;
  }
  if (UNLIKELY(it.HasNext() || *shorty != '\0')) {
    ErrorStringPrintf("Mismatched length for parameters and shorty");
    return false;
  }

  // Check ordering between items. This relies on type_ids being in order.
  if (previous_item_ != nullptr) {
    const dex::ProtoId* prev = reinterpret_cast<const dex::ProtoId*>(previous_item_);
    if (UNLIKELY(prev->return_type_idx_ > item->return_type_idx_)) {
      ErrorStringPrintf("Out-of-order proto_id return types");
      return false;
    } else if (prev->return_type_idx_ == item->return_type_idx_) {
      DexFileParameterIterator curr_it(*dex_file_, *item);
      DexFileParameterIterator prev_it(*dex_file_, *prev);

      while (curr_it.HasNext() && prev_it.HasNext()) {
        dex::TypeIndex prev_idx = prev_it.GetTypeIdx();
        dex::TypeIndex curr_idx = curr_it.GetTypeIdx();
        DCHECK_NE(prev_idx, dex::TypeIndex(DexFile::kDexNoIndex16));
        DCHECK_NE(curr_idx, dex::TypeIndex(DexFile::kDexNoIndex16));

        if (prev_idx < curr_idx) {
          break;
        } else if (UNLIKELY(prev_idx > curr_idx)) {
          ErrorStringPrintf("Out-of-order proto_id arguments");
          return false;
        }

        prev_it.Next();
        curr_it.Next();
      }
      if (!curr_it.HasNext()) {
        // Either a duplicate ProtoId or a ProtoId with a shorter argument list follows
        // a ProtoId with a longer one. Both cases are forbidden by the specification.
        ErrorStringPrintf("Out-of-order proto_id arguments");
        return false;
      }
    }
  }

  ptr_ += sizeof(dex::ProtoId);
  return true;
}

bool DexFileVerifier::CheckInterFieldIdItem() {
  const dex::FieldId* item = reinterpret_cast<const dex::FieldId*>(ptr_);

  // Check that the class descriptor is valid.
  if (UNLIKELY(!VerifyTypeDescriptor(item->class_idx_,
                                     "Invalid descriptor for class_idx",
                                     [](char d) { return d == 'L'; }))) {
    return false;
  }

  // Check that the type descriptor is a valid field name.
  if (UNLIKELY(!VerifyTypeDescriptor(item->type_idx_,
                                     "Invalid descriptor for type_idx",
                                     [](char d) { return d != 'V'; }))) {
    return false;
  }

  // Check that the name is valid.
  const char* field_name = dex_file_->StringDataByIdx(item->name_idx_);
  if (UNLIKELY(!IsValidMemberName(field_name))) {
    ErrorStringPrintf("Invalid field name: '%s'", field_name);
    return false;
  }

  // Check ordering between items. This relies on the other sections being in order.
  if (previous_item_ != nullptr) {
    const dex::FieldId* prev_item = reinterpret_cast<const dex::FieldId*>(previous_item_);
    if (UNLIKELY(prev_item->class_idx_ > item->class_idx_)) {
      ErrorStringPrintf("Out-of-order field_ids");
      return false;
    } else if (prev_item->class_idx_ == item->class_idx_) {
      if (UNLIKELY(prev_item->name_idx_ > item->name_idx_)) {
        ErrorStringPrintf("Out-of-order field_ids");
        return false;
      } else if (prev_item->name_idx_ == item->name_idx_) {
        if (UNLIKELY(prev_item->type_idx_ >= item->type_idx_)) {
          ErrorStringPrintf("Out-of-order field_ids");
          return false;
        }
      }
    }
  }

  ptr_ += sizeof(dex::FieldId);
  return true;
}

bool DexFileVerifier::CheckInterMethodIdItem() {
  const dex::MethodId* item = reinterpret_cast<const dex::MethodId*>(ptr_);

  // Check that the class descriptor is a valid reference name.
  if (UNLIKELY(!VerifyTypeDescriptor(item->class_idx_,
                                     "Invalid descriptor for class_idx",
                                     [](char d) { return d == 'L' || d == '['; }))) {
    return false;
  }

  // Check that the name is valid.
  const char* method_name = dex_file_->StringDataByIdx(item->name_idx_);
  if (UNLIKELY(!IsValidMemberName(method_name))) {
    ErrorStringPrintf("Invalid method name: '%s'", method_name);
    return false;
  }

  // Check that the proto id is valid.
  if (UNLIKELY(!CheckIndex(item->proto_idx_.index_, dex_file_->NumProtoIds(),
                           "inter_method_id_item proto_idx"))) {
    return false;
  }

  // Check ordering between items. This relies on the other sections being in order.
  if (previous_item_ != nullptr) {
    const dex::MethodId* prev_item = reinterpret_cast<const dex::MethodId*>(previous_item_);
    if (UNLIKELY(prev_item->class_idx_ > item->class_idx_)) {
      ErrorStringPrintf("Out-of-order method_ids");
      return false;
    } else if (prev_item->class_idx_ == item->class_idx_) {
      if (UNLIKELY(prev_item->name_idx_ > item->name_idx_)) {
        ErrorStringPrintf("Out-of-order method_ids");
        return false;
      } else if (prev_item->name_idx_ == item->name_idx_) {
        if (UNLIKELY(prev_item->proto_idx_ >= item->proto_idx_)) {
          ErrorStringPrintf("Out-of-order method_ids");
          return false;
        }
      }
    }
  }

  ptr_ += sizeof(dex::MethodId);
  return true;
}

bool DexFileVerifier::CheckInterClassDefItem() {
  const dex::ClassDef* item = reinterpret_cast<const dex::ClassDef*>(ptr_);

  // Check that class_idx_ is representable as a uint16_t;
  if (UNLIKELY(!IsValidTypeId(item->class_idx_.index_, item->pad1_))) {
    ErrorStringPrintf("class with type idx outside uint16_t range '%x:%x'", item->pad1_,
                      item->class_idx_.index_);
    return false;
  }
  // Check that superclass_idx_ is representable as a uint16_t;
  if (UNLIKELY(!IsValidOrNoTypeId(item->superclass_idx_.index_, item->pad2_))) {
    ErrorStringPrintf("class with superclass type idx outside uint16_t range '%x:%x'", item->pad2_,
                      item->superclass_idx_.index_);
    return false;
  }
  // Check for duplicate class def.

  if (UNLIKELY(!VerifyTypeDescriptor(item->class_idx_,
                                     "Invalid class descriptor",
                                     [](char d) { return d == 'L'; }))) {
    return false;
  }

  // Only allow non-runtime modifiers.
  if ((item->access_flags_ & ~kAccJavaFlagsMask) != 0) {
    ErrorStringPrintf("Invalid class flags: '%d'", item->access_flags_);
    return false;
  }

  if (item->interfaces_off_ != 0 &&
      !CheckOffsetToTypeMap(item->interfaces_off_, DexFile::kDexTypeTypeList)) {
    return false;
  }
  if (item->annotations_off_ != 0 &&
      !CheckOffsetToTypeMap(item->annotations_off_, DexFile::kDexTypeAnnotationsDirectoryItem)) {
    return false;
  }
  if (item->class_data_off_ != 0 &&
      !CheckOffsetToTypeMap(item->class_data_off_, DexFile::kDexTypeClassDataItem)) {
    return false;
  }
  if (item->static_values_off_ != 0 &&
      !CheckOffsetToTypeMap(item->static_values_off_, DexFile::kDexTypeEncodedArrayItem)) {
    return false;
  }

  if (item->superclass_idx_.IsValid()) {
    if (header_->GetVersion() >= DexFile::kClassDefinitionOrderEnforcedVersion) {
      // Check that a class does not inherit from itself directly (by having
      // the same type idx as its super class).
      if (UNLIKELY(item->superclass_idx_ == item->class_idx_)) {
        ErrorStringPrintf("Class with same type idx as its superclass: '%d'",
                          item->class_idx_.index_);
        return false;
      }

      // Check that a class is defined after its super class (if the
      // latter is defined in the same Dex file).
      uint16_t superclass_idx = item->superclass_idx_.index_;
      if (defined_classes_[superclass_idx]) {
        // The superclass is defined in this Dex file.
        if (&dex_file_->GetClassDef(defined_class_indexes_[superclass_idx]) > item) {
          // ClassDef item for super class appearing after the class' ClassDef item.
          ErrorStringPrintf("Invalid class definition ordering:"
                            " class with type idx: '%d' defined before"
                            " superclass with type idx: '%d'",
                            item->class_idx_.index_,
                            superclass_idx);
          return false;
        }
      }
    }

    if (UNLIKELY(!VerifyTypeDescriptor(item->superclass_idx_,
                                       "Invalid superclass",
                                       [](char d) { return d == 'L'; }))) {
      return false;
    }
  }

  // Check interfaces.
  const dex::TypeList* interfaces = dex_file_->GetInterfacesList(*item);
  if (interfaces != nullptr) {
    uint32_t size = interfaces->Size();
    for (uint32_t i = 0; i < size; i++) {
      if (header_->GetVersion() >= DexFile::kClassDefinitionOrderEnforcedVersion) {
        // Check that a class does not implement itself directly (by having the
        // same type idx as one of its immediate implemented interfaces).
        if (UNLIKELY(interfaces->GetTypeItem(i).type_idx_ == item->class_idx_)) {
          ErrorStringPrintf("Class with same type idx as implemented interface: '%d'",
                            item->class_idx_.index_);
          return false;
        }

        // Check that a class is defined after the interfaces it implements
        // (if they are defined in the same Dex file).
        uint16_t interface_idx = interfaces->GetTypeItem(i).type_idx_.index_;
        if (defined_classes_[interface_idx]) {
          // The interface is defined in this Dex file.
          if (&dex_file_->GetClassDef(defined_class_indexes_[interface_idx]) > item) {
            // ClassDef item for interface appearing after the class' ClassDef item.
            ErrorStringPrintf("Invalid class definition ordering:"
                              " class with type idx: '%d' defined before"
                              " implemented interface with type idx: '%d'",
                              item->class_idx_.index_,
                              interface_idx);
            return false;
          }
        }
      }

      // Ensure that the interface refers to a class (not an array nor a primitive type).
      if (UNLIKELY(!VerifyTypeDescriptor(interfaces->GetTypeItem(i).type_idx_,
                                         "Invalid interface",
                                         [](char d) { return d == 'L'; }))) {
        return false;
      }
    }

    /*
     * Ensure that there are no duplicates. This is an O(N^2) test, but in
     * practice the number of interfaces implemented by any given class is low.
     */
    for (uint32_t i = 1; i < size; i++) {
      dex::TypeIndex idx1 = interfaces->GetTypeItem(i).type_idx_;
      for (uint32_t j =0; j < i; j++) {
        dex::TypeIndex idx2 = interfaces->GetTypeItem(j).type_idx_;
        if (UNLIKELY(idx1 == idx2)) {
          ErrorStringPrintf("Duplicate interface: '%s'", dex_file_->StringByTypeIdx(idx1));
          return false;
        }
      }
    }
  }

  // Check that references in class_data_item are to the right class.
  if (item->class_data_off_ != 0) {
    ClassAccessor accessor(*dex_file_, OffsetToPtr(item->class_data_off_));
    uint32_t data_definer = FindFirstClassDataDefiner(accessor);
    DCHECK(IsUint<16>(data_definer) || data_definer == kDexNoIndex) << data_definer;
    if (UNLIKELY((data_definer != item->class_idx_.index_) && (data_definer != kDexNoIndex))) {
      ErrorStringPrintf("Invalid class_data_item");
      return false;
    }
  }

  // Check that references in annotations_directory_item are to right class.
  if (item->annotations_off_ != 0) {
    // annotations_off_ is supposed to be aligned by 4.
    if (!IsAlignedParam(item->annotations_off_, 4)) {
      ErrorStringPrintf("Invalid annotations_off_, not aligned by 4");
      return false;
    }
    const uint8_t* data = OffsetToPtr(item->annotations_off_);
    uint32_t defining_class = FindFirstAnnotationsDirectoryDefiner(data);
    DCHECK(IsUint<16>(defining_class) || defining_class == kDexNoIndex) << defining_class;
    if (UNLIKELY((defining_class != item->class_idx_.index_) && (defining_class != kDexNoIndex))) {
      ErrorStringPrintf("Invalid annotations_directory_item");
      return false;
    }
  }

  ptr_ += sizeof(dex::ClassDef);
  return true;
}

bool DexFileVerifier::CheckInterCallSiteIdItem() {
  const dex::CallSiteIdItem* item = reinterpret_cast<const dex::CallSiteIdItem*>(ptr_);

  // Check call site referenced by item is in encoded array section.
  if (!CheckOffsetToTypeMap(item->data_off_, DexFile::kDexTypeEncodedArrayItem)) {
    DCHECK(!failure_reason_.empty());  // Error already set.
    return false;
  }

  CallSiteArrayValueIterator it(*dex_file_, *item);

  // Check Method Handle
  if (!it.HasNext() || it.GetValueType() != EncodedArrayValueIterator::ValueType::kMethodHandle) {
    ErrorStringPrintf("CallSiteArray missing method handle");
    return false;
  }

  uint32_t handle_index = static_cast<uint32_t>(it.GetJavaValue().i);
  if (handle_index >= dex_file_->NumMethodHandles()) {
    ErrorStringPrintf("CallSite has bad method handle id: %x", handle_index);
    return false;
  }

  // Check target method name.
  if (!it.MaybeNext()) {
    ErrorStringPrintf("unexpected encoded value type: '%c'", it.GetValueType());
    return false;
  }
  if (!it.HasNext() ||
      it.GetValueType() != EncodedArrayValueIterator::ValueType::kString) {
    ErrorStringPrintf("CallSiteArray missing target method name");
    return false;
  }

  uint32_t name_index = static_cast<uint32_t>(it.GetJavaValue().i);
  if (name_index >= dex_file_->NumStringIds()) {
    ErrorStringPrintf("CallSite has bad method name id: %x", name_index);
    return false;
  }

  // Check method type.
  if (!it.MaybeNext()) {
    ErrorStringPrintf("unexpected encoded value type: '%c'", it.GetValueType());
    return false;
  }
  if (!it.HasNext() ||
      it.GetValueType() != EncodedArrayValueIterator::ValueType::kMethodType) {
    ErrorStringPrintf("CallSiteArray missing method type");
    return false;
  }

  uint32_t proto_index = static_cast<uint32_t>(it.GetJavaValue().i);
  if (proto_index >= dex_file_->NumProtoIds()) {
    ErrorStringPrintf("CallSite has bad method type: %x", proto_index);
    return false;
  }

  ptr_ += sizeof(dex::CallSiteIdItem);
  return true;
}

bool DexFileVerifier::CheckInterAnnotationSetRefList() {
  const dex::AnnotationSetRefList* list = reinterpret_cast<const dex::AnnotationSetRefList*>(ptr_);
  const dex::AnnotationSetRefItem* item = list->list_;
  uint32_t count = list->size_;

  for (; count != 0u; --count) {
    if (item->annotations_off_ != 0 &&
        !CheckOffsetToTypeMap(item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    item++;
  }

  ptr_ = reinterpret_cast<const uint8_t*>(item);
  return true;
}

bool DexFileVerifier::CheckInterAnnotationSetItem() {
  const dex::AnnotationSetItem* set = reinterpret_cast<const dex::AnnotationSetItem*>(ptr_);
  const uint32_t* offsets = set->entries_;
  uint32_t count = set->size_;
  uint32_t last_idx = 0;

  for (uint32_t i = 0; i < count; i++) {
    if (*offsets != 0 && !CheckOffsetToTypeMap(*offsets, DexFile::kDexTypeAnnotationItem)) {
      return false;
    }

    // Get the annotation from the offset and the type index for the annotation.
    const dex::AnnotationItem* annotation = OffsetToPtr<dex::AnnotationItem>(*offsets);
    const uint8_t* data = annotation->annotation_;
    DECODE_UNSIGNED_CHECKED_FROM(data, idx);

    if (UNLIKELY(last_idx >= idx && i != 0)) {
      ErrorStringPrintf("Out-of-order entry types: %x then %x", last_idx, idx);
      return false;
    }

    last_idx = idx;
    offsets++;
  }

  ptr_ = reinterpret_cast<const uint8_t*>(offsets);
  return true;
}

bool DexFileVerifier::CheckInterClassDataItem() {
  ClassAccessor accessor(*dex_file_, ptr_);
  uint32_t defining_class = FindFirstClassDataDefiner(accessor);
  DCHECK(IsUint<16>(defining_class) || defining_class == kDexNoIndex) << defining_class;
  if (defining_class == kDexNoIndex) {
    return true;  // Empty definitions are OK (but useless) and could be shared by multiple classes.
  }
  if (!defined_classes_[defining_class]) {
      // Should really have a class definition for this class data item.
      ErrorStringPrintf("Could not find declaring class for non-empty class data item.");
      return false;
  }
  const dex::TypeIndex class_type_index(defining_class);
  const dex::ClassDef& class_def = dex_file_->GetClassDef(defined_class_indexes_[defining_class]);

  for (const ClassAccessor::Field& read_field : accessor.GetFields()) {
    // The index has already been checked in `CheckIntraClassDataItemFields()`.
    DCHECK_LE(read_field.GetIndex(), header_->field_ids_size_);
    const dex::FieldId& field = dex_file_->GetFieldId(read_field.GetIndex());
    if (UNLIKELY(field.class_idx_ != class_type_index)) {
      ErrorStringPrintf("Mismatched defining class for class_data_item field");
      return false;
    }
    if (!CheckClassDataItemField(read_field.GetIndex(),
                                 read_field.GetAccessFlags(),
                                 class_def.access_flags_,
                                 class_type_index)) {
      return false;
    }
  }
  size_t num_direct_methods = accessor.NumDirectMethods();
  size_t num_processed_methods = 0u;
  auto methods = accessor.GetMethods();
  auto it = methods.begin();
  for (; it != methods.end(); ++it, ++num_processed_methods) {
    uint32_t code_off = it->GetCodeItemOffset();
    if (code_off != 0 && !CheckOffsetToTypeMap(code_off, DexFile::kDexTypeCodeItem)) {
      return false;
    }
    // The index has already been checked in `CheckIntraClassDataItemMethods()`.
    DCHECK_LE(it->GetIndex(), header_->method_ids_size_);
    const dex::MethodId& method = dex_file_->GetMethodId(it->GetIndex());
    if (UNLIKELY(method.class_idx_ != class_type_index)) {
      ErrorStringPrintf("Mismatched defining class for class_data_item method");
      return false;
    }
    bool expect_direct = num_processed_methods < num_direct_methods;
    if (!CheckClassDataItemMethod(it->GetIndex(),
                                  it->GetAccessFlags(),
                                  class_def.access_flags_,
                                  class_type_index,
                                  it->GetCodeItemOffset(),
                                  expect_direct)) {
      return false;
    }
  }

  // Check static field types against initial static values in encoded array.
  if (!CheckStaticFieldTypes(class_def)) {
    return false;
  }

  ptr_ = it.GetDataPointer();
  return true;
}

bool DexFileVerifier::CheckInterAnnotationsDirectoryItem() {
  const dex::AnnotationsDirectoryItem* item =
      reinterpret_cast<const dex::AnnotationsDirectoryItem*>(ptr_);
  uint32_t defining_class = FindFirstAnnotationsDirectoryDefiner(ptr_);
  DCHECK(IsUint<16>(defining_class) || defining_class == kDexNoIndex) << defining_class;

  if (item->class_annotations_off_ != 0 &&
      !CheckOffsetToTypeMap(item->class_annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
    return false;
  }

  // Field annotations follow immediately after the annotations directory.
  const dex::FieldAnnotationsItem* field_item =
      reinterpret_cast<const dex::FieldAnnotationsItem*>(item + 1);
  uint32_t field_count = item->fields_size_;
  for (uint32_t i = 0; i < field_count; i++) {
    // The index has already been checked in `CheckIntraAnnotationsDirectoryItem()`.
    DCHECK_LE(field_item->field_idx_, header_->field_ids_size_);
    const dex::FieldId& field = dex_file_->GetFieldId(field_item->field_idx_);
    if (UNLIKELY(field.class_idx_.index_ != defining_class)) {
      ErrorStringPrintf("Mismatched defining class for field_annotation");
      return false;
    }
    if (!CheckOffsetToTypeMap(field_item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    field_item++;
  }

  // Method annotations follow immediately after field annotations.
  const dex::MethodAnnotationsItem* method_item =
      reinterpret_cast<const dex::MethodAnnotationsItem*>(field_item);
  uint32_t method_count = item->methods_size_;
  for (uint32_t i = 0; i < method_count; i++) {
    // The index has already been checked in `CheckIntraAnnotationsDirectoryItem()`.
    DCHECK_LE(method_item->method_idx_, header_->method_ids_size_);
    const dex::MethodId& method = dex_file_->GetMethodId(method_item->method_idx_);
    if (UNLIKELY(method.class_idx_.index_ != defining_class)) {
      ErrorStringPrintf("Mismatched defining class for method_annotation");
      return false;
    }
    if (!CheckOffsetToTypeMap(method_item->annotations_off_, DexFile::kDexTypeAnnotationSetItem)) {
      return false;
    }
    method_item++;
  }

  // Parameter annotations follow immediately after method annotations.
  const dex::ParameterAnnotationsItem* parameter_item =
      reinterpret_cast<const dex::ParameterAnnotationsItem*>(method_item);
  uint32_t parameter_count = item->parameters_size_;
  for (uint32_t i = 0; i < parameter_count; i++) {
    // The index has already been checked in `CheckIntraAnnotationsDirectoryItem()`.
    DCHECK_LE(parameter_item->method_idx_, header_->method_ids_size_);
    const dex::MethodId& parameter_method = dex_file_->GetMethodId(parameter_item->method_idx_);
    if (UNLIKELY(parameter_method.class_idx_.index_ != defining_class)) {
      ErrorStringPrintf("Mismatched defining class for parameter_annotation");
      return false;
    }
    if (!CheckOffsetToTypeMap(parameter_item->annotations_off_,
        DexFile::kDexTypeAnnotationSetRefList)) {
      return false;
    }
    parameter_item++;
  }

  ptr_ = reinterpret_cast<const uint8_t*>(parameter_item);
  return true;
}

bool DexFileVerifier::CheckInterSectionIterate(size_t offset,
                                               uint32_t count,
                                               DexFile::MapItemType type) {
  // Get the right alignment mask for the type of section.
  size_t alignment_mask;
  switch (type) {
    case DexFile::kDexTypeClassDataItem:
      alignment_mask = sizeof(uint8_t) - 1;
      break;
    default:
      alignment_mask = sizeof(uint32_t) - 1;
      break;
  }

  // Iterate through the items in the section.
  previous_item_ = nullptr;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t new_offset = (offset + alignment_mask) & ~alignment_mask;
    ptr_ = OffsetToPtr(new_offset);
    const uint8_t* prev_ptr = ptr_;

    if (MapTypeToBitMask(type) == 0) {
      ErrorStringPrintf("Unknown map item type %x", type);
      return false;
    }

    // Check depending on the section type.
    switch (type) {
      case DexFile::kDexTypeHeaderItem:
      case DexFile::kDexTypeMethodHandleItem:
      case DexFile::kDexTypeMapList:
      case DexFile::kDexTypeTypeList:
      case DexFile::kDexTypeCodeItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
        break;
      case DexFile::kDexTypeHiddenapiClassData: {
        if (!CheckIntraHiddenapiClassData()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeStringIdItem: {
        if (!CheckInterStringIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeTypeIdItem: {
        if (!CheckInterTypeIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeProtoIdItem: {
        if (!CheckInterProtoIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeFieldIdItem: {
        if (!CheckInterFieldIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeMethodIdItem: {
        if (!CheckInterMethodIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDefItem: {
        // There shouldn't be more class definitions than type ids allow.
        // This is checked in `CheckIntraClassDefItem()` by checking the type
        // index against `kTypeIdLimit` and rejecting dulicate definitions.
        DCHECK_LE(i, kTypeIdLimit);
        if (!CheckInterClassDefItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeCallSiteIdItem: {
        if (!CheckInterCallSiteIdItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetRefList: {
        if (!CheckInterAnnotationSetRefList()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationSetItem: {
        if (!CheckInterAnnotationSetItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeClassDataItem: {
        // There shouldn't be more class data than type ids allow.
        // This check should be redundant, since there are checks that the
        // class_idx_ is within range and that there is only one definition
        // for a given type id.
        if (i > kTypeIdLimit) {
          ErrorStringPrintf("Too many class data items");
          return false;
        }
        if (!CheckInterClassDataItem()) {
          return false;
        }
        break;
      }
      case DexFile::kDexTypeAnnotationsDirectoryItem: {
        if (!CheckInterAnnotationsDirectoryItem()) {
          return false;
        }
        break;
      }
    }

    previous_item_ = prev_ptr;
    offset = PtrToOffset(ptr_);
  }

  return true;
}

bool DexFileVerifier::CheckInterSection() {
  // Eagerly verify that `StringId` offsets map to string data items to make sure
  // we can retrieve the string data for verifying other items (types, shorties, etc.).
  // After this we can safely use `DexFile` helpers such as `GetFieldId()` or `GetMethodId()`
  // but not `PrettyMethod()` or `PrettyField()` as descriptors have not been verified yet.
  const dex::StringId* string_ids = OffsetToPtr<dex::StringId>(header_->string_ids_off_);
  for (size_t i = 0, num_strings = header_->string_ids_size_; i != num_strings; ++i) {
    if (!CheckOffsetToTypeMap(string_ids[i].string_data_off_, DexFile::kDexTypeStringDataItem)) {
      return false;
    }
  }

  const dex::MapList* map = OffsetToPtr<dex::MapList>(header_->map_off_);
  const dex::MapItem* item = map->list_;
  uint32_t count = map->size_;

  // Cross check the items listed in the map.
  for (; count != 0u; --count) {
    uint32_t section_offset = item->offset_;
    uint32_t section_count = item->size_;
    DexFile::MapItemType type = static_cast<DexFile::MapItemType>(item->type_);
    bool found = false;

    if (type == DexFile::kDexTypeClassDataItem) {
      FindStringRangesForMethodNames();
    }

    switch (type) {
      case DexFile::kDexTypeHeaderItem:
      case DexFile::kDexTypeMapList:
      case DexFile::kDexTypeTypeList:
      case DexFile::kDexTypeCodeItem:
      case DexFile::kDexTypeStringDataItem:
      case DexFile::kDexTypeDebugInfoItem:
      case DexFile::kDexTypeAnnotationItem:
      case DexFile::kDexTypeEncodedArrayItem:
        found = true;
        break;
      case DexFile::kDexTypeStringIdItem:
      case DexFile::kDexTypeTypeIdItem:
      case DexFile::kDexTypeProtoIdItem:
      case DexFile::kDexTypeFieldIdItem:
      case DexFile::kDexTypeMethodIdItem:
      case DexFile::kDexTypeClassDefItem:
      case DexFile::kDexTypeCallSiteIdItem:
      case DexFile::kDexTypeMethodHandleItem:
      case DexFile::kDexTypeAnnotationSetRefList:
      case DexFile::kDexTypeAnnotationSetItem:
      case DexFile::kDexTypeClassDataItem:
      case DexFile::kDexTypeAnnotationsDirectoryItem:
      case DexFile::kDexTypeHiddenapiClassData: {
        if (!CheckInterSectionIterate(section_offset, section_count, type)) {
          return false;
        }
        found = true;
        break;
      }
    }

    if (!found) {
      ErrorStringPrintf("Unknown map item type %x", item->type_);
      return false;
    }

    item++;
  }

  return true;
}

bool DexFileVerifier::Verify() {
  // Check the header.
  if (!CheckHeader()) {
    return false;
  }

  // Check the map section.
  if (!CheckMap()) {
    return false;
  }

  DCHECK_LE(header_->type_ids_size_, kTypeIdLimit + 1u);  // Checked in CheckHeader().
  verified_type_descriptors_.resize(header_->type_ids_size_, 0);
  defined_class_indexes_.resize(header_->type_ids_size_);

  // Check structure within remaining sections.
  if (!CheckIntraSection()) {
    return false;
  }

  // Check references from one section to another.
  if (!CheckInterSection()) {
    return false;
  }

  return true;
}

bool DexFileVerifier::CheckFieldAccessFlags(uint32_t idx,
                                            uint32_t field_access_flags,
                                            uint32_t class_access_flags,
                                            std::string* error_msg) {
  // Generally sort out >16-bit flags.
  if ((field_access_flags & ~kAccJavaFlagsMask) != 0) {
    *error_msg = StringPrintf("Bad field access_flags for %s: %x(%s)",
                              GetFieldDescription(idx).c_str(),
                              field_access_flags,
                              PrettyJavaAccessFlags(field_access_flags).c_str());
    return false;
  }

  // Flags allowed on fields, in general. Other lower-16-bit flags are to be ignored.
  constexpr uint32_t kFieldAccessFlags = kAccPublic |
                                         kAccPrivate |
                                         kAccProtected |
                                         kAccStatic |
                                         kAccFinal |
                                         kAccVolatile |
                                         kAccTransient |
                                         kAccSynthetic |
                                         kAccEnum;

  // Fields may have only one of public/protected/final.
  if (!CheckAtMostOneOfPublicProtectedPrivate(field_access_flags)) {
    *error_msg = StringPrintf("Field may have only one of public/protected/private, %s: %x(%s)",
                              GetFieldDescription(idx).c_str(),
                              field_access_flags,
                              PrettyJavaAccessFlags(field_access_flags).c_str());
    return false;
  }

  // Interfaces have a pretty restricted list.
  if ((class_access_flags & kAccInterface) != 0) {
    // Interface fields must be public final static.
    constexpr uint32_t kPublicFinalStatic = kAccPublic | kAccFinal | kAccStatic;
    if ((field_access_flags & kPublicFinalStatic) != kPublicFinalStatic) {
      *error_msg = StringPrintf("Interface field is not public final static, %s: %x(%s)",
                                GetFieldDescription(idx).c_str(),
                                field_access_flags,
                                PrettyJavaAccessFlags(field_access_flags).c_str());
      if (dex_file_->SupportsDefaultMethods()) {
        return false;
      } else {
        // Allow in older versions, but warn.
        LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                     << *error_msg;
      }
    }
    // Interface fields may be synthetic, but may not have other flags.
    constexpr uint32_t kDisallowed = ~(kPublicFinalStatic | kAccSynthetic);
    if ((field_access_flags & kFieldAccessFlags & kDisallowed) != 0) {
      *error_msg = StringPrintf("Interface field has disallowed flag, %s: %x(%s)",
                                GetFieldDescription(idx).c_str(),
                                field_access_flags,
                                PrettyJavaAccessFlags(field_access_flags).c_str());
      if (dex_file_->SupportsDefaultMethods()) {
        return false;
      } else {
        // Allow in older versions, but warn.
        LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                     << *error_msg;
      }
    }
    return true;
  }

  // Volatile fields may not be final.
  constexpr uint32_t kVolatileFinal = kAccVolatile | kAccFinal;
  if ((field_access_flags & kVolatileFinal) == kVolatileFinal) {
    *error_msg = StringPrintf("Fields may not be volatile and final: %s",
                              GetFieldDescription(idx).c_str());
    return false;
  }

  return true;
}

void DexFileVerifier::FindStringRangesForMethodNames() {
  // Use DexFile::StringId* as RandomAccessIterator.
  const dex::StringId* first = OffsetToPtr<dex::StringId>(header_->string_ids_off_);
  const dex::StringId* last = first + header_->string_ids_size_;

  auto get_string = [this](const dex::StringId& id) {
    const uint8_t* str_data_ptr = OffsetToPtr(id.string_data_off_);
    DecodeUnsignedLeb128(&str_data_ptr);
    return reinterpret_cast<const char*>(str_data_ptr);
  };
  auto compare = [&get_string](const dex::StringId& lhs, const char* rhs) {
    return CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(get_string(lhs), rhs) < 0;
  };

  // '=' follows '<'
  static_assert('<' + 1 == '=', "Unexpected character relation");
  const auto angle_end = std::lower_bound(first, last, "=", compare);
  init_indices_.angle_bracket_end_index = angle_end - first;

  const auto angle_start = std::lower_bound(first, angle_end, "<", compare);
  init_indices_.angle_bracket_start_index = angle_start - first;
  if (angle_start == angle_end) {
    // No strings starting with '<'.
    init_indices_.angle_init_angle_index = std::numeric_limits<size_t>::max();
    init_indices_.angle_clinit_angle_index = std::numeric_limits<size_t>::max();
    return;
  }

  {
    constexpr const char* kClinit = "<clinit>";
    const auto it = std::lower_bound(angle_start, angle_end, kClinit, compare);
    if (it != angle_end && strcmp(get_string(*it), kClinit) == 0) {
      init_indices_.angle_clinit_angle_index = it - first;
    } else {
      init_indices_.angle_clinit_angle_index = std::numeric_limits<size_t>::max();
    }
  }
  {
    constexpr const char* kInit = "<init>";
    const auto it = std::lower_bound(angle_start, angle_end, kInit, compare);
    if (it != angle_end && strcmp(get_string(*it), kInit) == 0) {
      init_indices_.angle_init_angle_index = it - first;
    } else {
      init_indices_.angle_init_angle_index = std::numeric_limits<size_t>::max();
    }
  }
}

bool DexFileVerifier::CheckMethodAccessFlags(uint32_t method_index,
                                             uint32_t method_access_flags,
                                             uint32_t class_access_flags,
                                             uint32_t constructor_flags_by_name,
                                             bool has_code,
                                             bool expect_direct,
                                             std::string* error_msg) {
  // Generally sort out >16-bit flags, except dex knows Constructor and DeclaredSynchronized.
  constexpr uint32_t kAllMethodFlags =
      kAccJavaFlagsMask | kAccConstructor | kAccDeclaredSynchronized;
  if ((method_access_flags & ~kAllMethodFlags) != 0) {
    *error_msg = StringPrintf("Bad method access_flags for %s: %x",
                              GetMethodDescription(method_index).c_str(),
                              method_access_flags);
    return false;
  }

  // Flags allowed on methods, in general. Other lower-16-bit flags are to be ignored.
  constexpr uint32_t kMethodAccessFlags = kAccPublic |
                                          kAccPrivate |
                                          kAccProtected |
                                          kAccStatic |
                                          kAccFinal |
                                          kAccSynthetic |
                                          kAccSynchronized |
                                          kAccBridge |
                                          kAccVarargs |
                                          kAccNative |
                                          kAccAbstract |
                                          kAccStrict;

  // Methods may have only one of public/protected/final.
  if (!CheckAtMostOneOfPublicProtectedPrivate(method_access_flags)) {
    *error_msg = StringPrintf("Method may have only one of public/protected/private, %s: %x",
                              GetMethodDescription(method_index).c_str(),
                              method_access_flags);
    return false;
  }

  constexpr uint32_t kConstructorFlags = kAccStatic | kAccConstructor;
  const bool is_constructor_by_name = (constructor_flags_by_name & kConstructorFlags) != 0;
  const bool is_clinit_by_name = constructor_flags_by_name == kConstructorFlags;

  // Only methods named "<clinit>" or "<init>" may be marked constructor. Note: we cannot enforce
  // the reverse for backwards compatibility reasons.
  if (((method_access_flags & kAccConstructor) != 0) && !is_constructor_by_name) {
    *error_msg =
        StringPrintf("Method %" PRIu32 "(%s) is marked constructor, but doesn't match name",
                     method_index,
                     GetMethodDescription(method_index).c_str());
    return false;
  }

  if (is_constructor_by_name) {
    // Check that the static constructor (= static initializer) is named "<clinit>" and that the
    // instance constructor is called "<init>".
    bool is_static = (method_access_flags & kAccStatic) != 0;
    if (is_static ^ is_clinit_by_name) {
      *error_msg = StringPrintf("Constructor %" PRIu32 "(%s) is not flagged correctly wrt/ static.",
                                method_index,
                                GetMethodDescription(method_index).c_str());
      if (dex_file_->SupportsDefaultMethods()) {
        return false;
      } else {
        // Allow in older versions, but warn.
        LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                     << *error_msg;
      }
    }
  }

  // Check that static and private methods, as well as constructors, are in the direct methods list,
  // and other methods in the virtual methods list.
  bool is_direct = ((method_access_flags & (kAccStatic | kAccPrivate)) != 0) ||
                   is_constructor_by_name;
  if (is_direct != expect_direct) {
    *error_msg = StringPrintf("Direct/virtual method %" PRIu32 "(%s) not in expected list %d",
                              method_index,
                              GetMethodDescription(method_index).c_str(),
                              expect_direct);
    return false;
  }

  // From here on out it is easier to mask out the bits we're supposed to ignore.
  method_access_flags &= kMethodAccessFlags;

  // Interfaces are special.
  if ((class_access_flags & kAccInterface) != 0) {
    // Non-static interface methods must be public or private.
    uint32_t desired_flags = (kAccPublic | kAccStatic);
    if (dex_file_->SupportsDefaultMethods()) {
      desired_flags |= kAccPrivate;
    }
    if ((method_access_flags & desired_flags) == 0) {
      *error_msg = StringPrintf("Interface virtual method %" PRIu32 "(%s) is not public",
                                method_index,
                                GetMethodDescription(method_index).c_str());
      if (dex_file_->SupportsDefaultMethods()) {
        return false;
      } else {
        // Allow in older versions, but warn.
        LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                      << *error_msg;
      }
    }
  }

  // If there aren't any instructions, make sure that's expected.
  if (!has_code) {
    // Only native or abstract methods may not have code.
    if ((method_access_flags & (kAccNative | kAccAbstract)) == 0) {
      *error_msg = StringPrintf("Method %" PRIu32 "(%s) has no code, but is not marked native or "
                                "abstract",
                                method_index,
                                GetMethodDescription(method_index).c_str());
      return false;
    }
    // Constructors must always have code.
    if (is_constructor_by_name) {
      *error_msg = StringPrintf("Constructor %u(%s) must not be abstract or native",
                                method_index,
                                GetMethodDescription(method_index).c_str());
      if (dex_file_->SupportsDefaultMethods()) {
        return false;
      } else {
        // Allow in older versions, but warn.
        LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                      << *error_msg;
      }
    }
    if ((method_access_flags & kAccAbstract) != 0) {
      // Abstract methods are not allowed to have the following flags.
      constexpr uint32_t kForbidden =
          kAccPrivate | kAccStatic | kAccFinal | kAccNative | kAccStrict | kAccSynchronized;
      if ((method_access_flags & kForbidden) != 0) {
        *error_msg = StringPrintf("Abstract method %" PRIu32 "(%s) has disallowed access flags %x",
                                  method_index,
                                  GetMethodDescription(method_index).c_str(),
                                  method_access_flags);
        return false;
      }
      // Abstract methods should be in an abstract class or interface.
      if ((class_access_flags & (kAccInterface | kAccAbstract)) == 0) {
        LOG(WARNING) << "Method " << GetMethodDescription(method_index)
                     << " is abstract, but the declaring class is neither abstract nor an "
                     << "interface in dex file "
                     << dex_file_->GetLocation();
      }
    }
    // Interfaces are special.
    if ((class_access_flags & kAccInterface) != 0) {
      // Interface methods without code must be abstract.
      if ((method_access_flags & (kAccPublic | kAccAbstract)) != (kAccPublic | kAccAbstract)) {
        *error_msg = StringPrintf("Interface method %" PRIu32 "(%s) is not public and abstract",
                                  method_index,
                                  GetMethodDescription(method_index).c_str());
        if (dex_file_->SupportsDefaultMethods()) {
          return false;
        } else {
          // Allow in older versions, but warn.
          LOG(WARNING) << "This dex file is invalid and will be rejected in the future. Error is: "
                       << *error_msg;
        }
      }
      // At this point, we know the method is public and abstract. This means that all the checks
      // for invalid combinations above applies. In addition, interface methods must not be
      // protected. This is caught by the check for only-one-of-public-protected-private.
    }
    return true;
  }

  // When there's code, the method must not be native or abstract.
  if ((method_access_flags & (kAccNative | kAccAbstract)) != 0) {
    *error_msg = StringPrintf("Method %" PRIu32 "(%s) has code, but is marked native or abstract",
                              method_index,
                              GetMethodDescription(method_index).c_str());
    return false;
  }

  // Instance constructors must not be synchronized and a few other flags.
  if (constructor_flags_by_name == kAccConstructor) {
    static constexpr uint32_t kInitAllowed =
        kAccPrivate | kAccProtected | kAccPublic | kAccStrict | kAccVarargs | kAccSynthetic;
    if ((method_access_flags & ~kInitAllowed) != 0) {
      *error_msg = StringPrintf("Constructor %" PRIu32 "(%s) flagged inappropriately %x",
                                method_index,
                                GetMethodDescription(method_index).c_str(),
                                method_access_flags);
      return false;
    }
  }

  return true;
}

bool DexFileVerifier::CheckConstructorProperties(
      uint32_t method_index,
      uint32_t constructor_flags) {
  DCHECK(constructor_flags == kAccConstructor ||
         constructor_flags == (kAccConstructor | kAccStatic));

  // Check signature matches expectations.
  // The `method_index` has already been checked in `CheckIntraClassDataItemMethods()`.
  CHECK_LT(method_index, header_->method_ids_size_);
  const dex::MethodId& method_id = dex_file_->GetMethodId(method_index);

  // The `method_id.proto_idx_` has already been checked in `CheckIntraMethodIdItem()`
  DCHECK_LE(method_id.proto_idx_.index_, header_->proto_ids_size_);

  Signature signature = dex_file_->GetMethodSignature(method_id);
  if (constructor_flags == (kAccStatic | kAccConstructor)) {
    if (!signature.IsVoid() || signature.GetNumberOfParameters() != 0) {
      ErrorStringPrintf("<clinit> must have descriptor ()V");
      return false;
    }
  } else if (!signature.IsVoid()) {
    ErrorStringPrintf("Constructor %u(%s) must be void",
                      method_index,
                      GetMethodDescription(method_index).c_str());
    return false;
  }

  return true;
}

bool Verify(const DexFile* dex_file,
            const char* location,
            bool verify_checksum,
            std::string* error_msg) {
  std::unique_ptr<DexFileVerifier> verifier(
      new DexFileVerifier(dex_file, location, verify_checksum));
  if (!verifier->Verify()) {
    *error_msg = verifier->FailureReason();
    return false;
  }
  return true;
}

}  // namespace dex
}  // namespace art
