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

#include "dex_file.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <type_traits>

#include "android-base/stringprintf.h"
#include "base/enums.h"
#include "base/hiddenapi_domain.h"
#include "base/leb128.h"
#include "base/stl_util.h"
#include "class_accessor-inl.h"
#include "compact_dex_file.h"
#include "descriptors_names.h"
#include "dex_file-inl.h"
#include "standard_dex_file.h"
#include "utf-inl.h"

namespace art {

using android::base::StringPrintf;

using dex::CallSiteIdItem;
using dex::ClassDef;
using dex::FieldId;
using dex::MapList;
using dex::MapItem;
using dex::MethodHandleItem;
using dex::MethodId;
using dex::ProtoId;
using dex::StringId;
using dex::TryItem;
using dex::TypeId;
using dex::TypeList;

static_assert(sizeof(dex::StringIndex) == sizeof(uint32_t), "StringIndex size is wrong");
static_assert(std::is_trivially_copyable<dex::StringIndex>::value, "StringIndex not trivial");
static_assert(sizeof(dex::TypeIndex) == sizeof(uint16_t), "TypeIndex size is wrong");
static_assert(std::is_trivially_copyable<dex::TypeIndex>::value, "TypeIndex not trivial");

// Print the SHA1 as 20-byte hexadecimal string.
std::string DexFile::Sha1::ToString() const {
  auto data = this->data();
  auto part = [d = data](int i) { return d[i] << 24 | d[i + 1] << 16 | d[i + 2] << 8 | d[i + 3]; };
  return StringPrintf("%08x%08x%08x%08x%08x", part(0), part(4), part(8), part(12), part(16));
}

uint32_t DexFile::CalculateChecksum() const {
  return CalculateChecksum(Begin(), Size());
}

uint32_t DexFile::CalculateChecksum(const uint8_t* begin, size_t size) {
  const uint32_t non_sum_bytes = OFFSETOF_MEMBER(DexFile::Header, signature_);
  return ChecksumMemoryRange(begin + non_sum_bytes, size - non_sum_bytes);
}

uint32_t DexFile::ChecksumMemoryRange(const uint8_t* begin, size_t size) {
  return adler32(adler32(0L, Z_NULL, 0), begin, size);
}

bool DexFile::IsReadOnly() const {
  CHECK(container_.get() != nullptr);
  return container_->IsReadOnly();
}

bool DexFile::EnableWrite() const {
  CHECK(container_.get() != nullptr);
  return container_->EnableWrite();
}

bool DexFile::DisableWrite() const {
  CHECK(container_.get() != nullptr);
  return container_->DisableWrite();
}

uint32_t DexFile::Header::GetExpectedHeaderSize() const {
  uint32_t version = GetVersion();
  return version == 0 ? 0 : version < 41 ? sizeof(Header) : sizeof(HeaderV41);
}

bool DexFile::Header::HasDexContainer() const {
  if (CompactDexFile::IsMagicValid(magic_.data())) {
    return false;
  }
  DCHECK_EQ(header_size_, GetExpectedHeaderSize());
  return header_size_ >= sizeof(HeaderV41);
}

uint32_t DexFile::Header::HeaderOffset() const {
  return HasDexContainer() ? reinterpret_cast<const HeaderV41*>(this)->header_offset_ : 0;
}

uint32_t DexFile::Header::ContainerSize() const {
  return HasDexContainer() ? reinterpret_cast<const HeaderV41*>(this)->container_size_ : file_size_;
}

void DexFile::Header::SetDexContainer(size_t header_offset, size_t container_size) {
  if (HasDexContainer()) {
    DCHECK_LE(header_offset, container_size);
    DCHECK_LE(file_size_, container_size - header_offset);
    data_off_ = 0;
    data_size_ = 0;
    auto* headerV41 = reinterpret_cast<HeaderV41*>(this);
    DCHECK_GE(header_size_, sizeof(*headerV41));
    headerV41->header_offset_ = header_offset;
    headerV41->container_size_ = container_size;
  } else {
    DCHECK_EQ(header_offset, 0u);
    DCHECK_EQ(container_size, file_size_);
  }
}

template <typename T>
ALWAYS_INLINE const T* DexFile::GetSection(const uint32_t* offset, DexFileContainer* container) {
  size_t size = container->End() - begin_;
  if (size < sizeof(Header)) {
    return nullptr;  // Invalid dex file.
  }
  // Compact dex is inconsistent: section offsets are relative to the
  // header as opposed to the data section like all other its offsets.
  if (CompactDexFile::IsMagicValid(begin_)) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(header_);
    return reinterpret_cast<const T*>(data + *offset);
  }
  return reinterpret_cast<const T*>(data_.data() + *offset);
}

DexFile::DexFile(const uint8_t* base,
                 const std::string& location,
                 uint32_t location_checksum,
                 const OatDexFile* oat_dex_file,
                 std::shared_ptr<DexFileContainer> container,
                 bool is_compact_dex)
    : begin_(base),
      data_(GetDataRange(base, container.get())),
      location_(location),
      location_checksum_(location_checksum),
      header_(reinterpret_cast<const Header*>(base)),
      string_ids_(GetSection<StringId>(&header_->string_ids_off_, container.get())),
      type_ids_(GetSection<TypeId>(&header_->type_ids_off_, container.get())),
      field_ids_(GetSection<FieldId>(&header_->field_ids_off_, container.get())),
      method_ids_(GetSection<MethodId>(&header_->method_ids_off_, container.get())),
      proto_ids_(GetSection<ProtoId>(&header_->proto_ids_off_, container.get())),
      class_defs_(GetSection<ClassDef>(&header_->class_defs_off_, container.get())),
      method_handles_(nullptr),
      num_method_handles_(0),
      call_site_ids_(nullptr),
      num_call_site_ids_(0),
      hiddenapi_class_data_(nullptr),
      oat_dex_file_(oat_dex_file),
      container_(std::move(container)),
      is_compact_dex_(is_compact_dex),
      hiddenapi_domain_(hiddenapi::Domain::kApplication) {
  CHECK(begin_ != nullptr) << GetLocation();
  // Check base (=header) alignment.
  // Must be 4-byte aligned to avoid undefined behavior when accessing
  // any of the sections via a pointer.
  CHECK_ALIGNED(begin_, alignof(Header));

  if (DataSize() < sizeof(Header)) {
    // Don't go further if the data doesn't even contain a header.
    return;
  }

  InitializeSectionsFromMapList();
}

DexFile::~DexFile() {
  // We don't call DeleteGlobalRef on dex_object_ because we're only called by DestroyJavaVM, and
  // that's only called after DetachCurrentThread, which means there's no JNIEnv. We could
  // re-attach, but cleaning up these global references is not obviously useful. It's not as if
  // the global reference table is otherwise empty!
}

bool DexFile::Init(std::string* error_msg) {
  CHECK_GE(container_->End(), reinterpret_cast<const uint8_t*>(header_));
  size_t container_size = container_->End() - reinterpret_cast<const uint8_t*>(header_);
  if (container_size < sizeof(Header)) {
    *error_msg = StringPrintf("Unable to open '%s' : File size is too small to fit dex header",
                              location_.c_str());
    return false;
  }
  if (!CheckMagicAndVersion(error_msg)) {
    return false;
  }
  if (!IsCompactDexFile()) {
    uint32_t expected_header_size = header_->GetExpectedHeaderSize();
    if (header_->header_size_ != expected_header_size) {
      *error_msg = StringPrintf("Unable to open '%s' : Header size is %u but %u was expected",
                                location_.c_str(),
                                header_->header_size_,
                                expected_header_size);
      return false;
    }
  }
  if (container_size < header_->file_size_) {
    *error_msg = StringPrintf("Unable to open '%s' : File size is %zu but the header expects %u",
                              location_.c_str(),
                              container_size,
                              header_->file_size_);
    return false;
  }
  return true;
}

bool DexFile::CheckMagicAndVersion(std::string* error_msg) const {
  if (!IsMagicValid()) {
    std::ostringstream oss;
    oss << "Unrecognized magic number in "  << GetLocation() << ":"
            << " " << header_->magic_[0]
            << " " << header_->magic_[1]
            << " " << header_->magic_[2]
            << " " << header_->magic_[3];
    *error_msg = oss.str();
    return false;
  }
  if (!IsVersionValid()) {
    std::ostringstream oss;
    oss << "Unrecognized version number in "  << GetLocation() << ":"
            << " " << header_->magic_[4]
            << " " << header_->magic_[5]
            << " " << header_->magic_[6]
            << " " << header_->magic_[7];
    *error_msg = oss.str();
    return false;
  }
  return true;
}

ArrayRef<const uint8_t> DexFile::GetDataRange(const uint8_t* data, DexFileContainer* container) {
  // NB: This function must survive random data to pass fuzzing and testing.
  CHECK(container != nullptr);
  CHECK_GE(data, container->Begin());
  CHECK_LE(data, container->End());
  size_t size = container->End() - data;
  if (size >= sizeof(StandardDexFile::Header) && StandardDexFile::IsMagicValid(data)) {
    auto header = reinterpret_cast<const DexFile::Header*>(data);
    CHECK_EQ(container->Data().size(), 0u) << "Unsupported for standard dex";
    if (size >= sizeof(HeaderV41) && header->header_size_ >= sizeof(HeaderV41)) {
      auto headerV41 = reinterpret_cast<const DexFile::HeaderV41*>(data);
      data -= headerV41->header_offset_;  // Allow underflow and later overflow.
      size = headerV41->container_size_;
    } else {
      size = header->file_size_;
    }
  } else if (size >= sizeof(CompactDexFile::Header) && CompactDexFile::IsMagicValid(data)) {
    auto header = reinterpret_cast<const CompactDexFile::Header*>(data);
    // TODO: Remove. This is a hack. See comment of the Data method.
    ArrayRef<const uint8_t> separate_data = container->Data();
    if (separate_data.size() > 0) {
      return separate_data;
    }
    // Shared compact dex data is located at the end after all dex files.
    data += std::min<size_t>(header->data_off_, size);
    size = header->data_size_;
  }
  // The returned range is guaranteed to be in bounds of the container memory.
  return {data, std::min<size_t>(size, container->End() - data)};
}

void DexFile::InitializeSectionsFromMapList() {
  // NB: This function must survive random data to pass fuzzing and testing.
  static_assert(sizeof(MapList) <= sizeof(Header));
  DCHECK_GE(DataSize(), sizeof(MapList));
  if (header_->map_off_ == 0 || header_->map_off_ > DataSize() - sizeof(MapList)) {
    // Bad offset. The dex file verifier runs after this method and will reject the file.
    return;
  }
  const uint8_t* map_list_raw = DataBegin() + header_->map_off_;
  if (map_list_raw < Begin()) {
    return;
  }
  const MapList* map_list = reinterpret_cast<const MapList*>(map_list_raw);
  const size_t count = map_list->size_;

  size_t map_limit =
      (DataSize() - OFFSETOF_MEMBER(MapList, list_) - header_->map_off_) / sizeof(MapItem);
  if (count > map_limit) {
    // Too many items. The dex file verifier runs after
    // this method and will reject the file as it is malformed.
    return;
  }

  // Construct pointers to certain arrays without any checks. If they are outside the
  // data, the dex file verification should fail and these pointers should not be used.
  for (size_t i = 0; i < count; ++i) {
    const MapItem& map_item = map_list->list_[i];
    if (map_item.type_ == kDexTypeMethodHandleItem) {
      method_handles_ = GetSection<MethodHandleItem>(&map_item.offset_, container_.get());
      num_method_handles_ = map_item.size_;
    } else if (map_item.type_ == kDexTypeCallSiteIdItem) {
      call_site_ids_ = GetSection<CallSiteIdItem>(&map_item.offset_, container_.get());
      num_call_site_ids_ = map_item.size_;
    } else if (map_item.type_ == kDexTypeHiddenapiClassData) {
      hiddenapi_class_data_ =
          reinterpret_cast<const dex::HiddenapiClassData*>(DataBegin() + map_item.offset_);
    } else {
      // Pointers to other sections are not necessary to retain in the DexFile struct.
      // Other items have pointers directly into their data.
    }
  }
}

uint32_t DexFile::Header::GetVersion() const {
  const char* version = reinterpret_cast<const char*>(&magic_[kDexMagicSize]);
  return atoi(version);
}

const ClassDef* DexFile::FindClassDef(dex::TypeIndex type_idx) const {
  size_t num_class_defs = NumClassDefs();
  // Fast path for rare no class defs case.
  if (num_class_defs == 0) {
    return nullptr;
  }
  for (size_t i = 0; i < num_class_defs; ++i) {
    const ClassDef& class_def = GetClassDef(i);
    if (class_def.class_idx_ == type_idx) {
      return &class_def;
    }
  }
  return nullptr;
}

std::optional<uint32_t> DexFile::GetCodeItemOffset(const ClassDef &class_def,
                                                   uint32_t method_idx) const {
  ClassAccessor accessor(*this, class_def);
  CHECK(accessor.HasClassData());
  for (const ClassAccessor::Method &method : accessor.GetMethods()) {
    if (method.GetIndex() == method_idx) {
      return method.GetCodeItemOffset();
    }
  }
  return std::nullopt;
}

uint32_t DexFile::FindCodeItemOffset(const dex::ClassDef &class_def,
                                     uint32_t dex_method_idx) const {
  std::optional<uint32_t> val = GetCodeItemOffset(class_def, dex_method_idx);
  CHECK(val.has_value()) << "Unable to find method " << dex_method_idx;
  return *val;
}

const FieldId* DexFile::FindFieldId(const TypeId& declaring_klass,
                                    const StringId& name,
                                    const TypeId& type) const {
  // Binary search MethodIds knowing that they are sorted by class_idx, name_idx then proto_idx
  const dex::TypeIndex class_idx = GetIndexForTypeId(declaring_klass);
  const dex::StringIndex name_idx = GetIndexForStringId(name);
  const dex::TypeIndex type_idx = GetIndexForTypeId(type);
  int32_t lo = 0;
  int32_t hi = NumFieldIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const FieldId& field = GetFieldId(mid);
    if (class_idx > field.class_idx_) {
      lo = mid + 1;
    } else if (class_idx < field.class_idx_) {
      hi = mid - 1;
    } else {
      if (name_idx > field.name_idx_) {
        lo = mid + 1;
      } else if (name_idx < field.name_idx_) {
        hi = mid - 1;
      } else {
        if (type_idx > field.type_idx_) {
          lo = mid + 1;
        } else if (type_idx < field.type_idx_) {
          hi = mid - 1;
        } else {
          return &field;
        }
      }
    }
  }
  return nullptr;
}

const MethodId* DexFile::FindMethodId(const TypeId& declaring_klass,
                                      const StringId& name,
                                      const ProtoId& signature) const {
  // Binary search MethodIds knowing that they are sorted by class_idx, name_idx then proto_idx
  const dex::TypeIndex class_idx = GetIndexForTypeId(declaring_klass);
  const dex::StringIndex name_idx = GetIndexForStringId(name);
  const dex::ProtoIndex proto_idx = GetIndexForProtoId(signature);
  return FindMethodIdByIndex(class_idx, name_idx, proto_idx);
}

const MethodId* DexFile::FindMethodIdByIndex(dex::TypeIndex class_idx,
                                             dex::StringIndex name_idx,
                                             dex::ProtoIndex proto_idx) const {
  // Binary search MethodIds knowing that they are sorted by class_idx, name_idx then proto_idx
  int32_t lo = 0;
  int32_t hi = NumMethodIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const MethodId& method = GetMethodId(mid);
    if (class_idx > method.class_idx_) {
      lo = mid + 1;
    } else if (class_idx < method.class_idx_) {
      hi = mid - 1;
    } else {
      if (name_idx > method.name_idx_) {
        lo = mid + 1;
      } else if (name_idx < method.name_idx_) {
        hi = mid - 1;
      } else {
        if (proto_idx > method.proto_idx_) {
          lo = mid + 1;
        } else if (proto_idx < method.proto_idx_) {
          hi = mid - 1;
        } else {
          DCHECK_EQ(class_idx, method.class_idx_);
          DCHECK_EQ(proto_idx, method.proto_idx_);
          DCHECK_EQ(name_idx, method.name_idx_);
          return &method;
        }
      }
    }
  }
  return nullptr;
}

const StringId* DexFile::FindStringId(const char* string) const {
  int32_t lo = 0;
  int32_t hi = NumStringIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const StringId& str_id = GetStringId(dex::StringIndex(mid));
    const char* str = GetStringData(str_id);
    int compare = CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(string, str);
    if (compare > 0) {
      lo = mid + 1;
    } else if (compare < 0) {
      hi = mid - 1;
    } else {
      return &str_id;
    }
  }
  return nullptr;
}

const TypeId* DexFile::FindTypeId(const char* string) const {
  int32_t lo = 0;
  int32_t hi = NumTypeIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const TypeId& type_id = GetTypeId(dex::TypeIndex(mid));
    const StringId& str_id = GetStringId(type_id.descriptor_idx_);
    const char* str = GetStringData(str_id);
    int compare = CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(string, str);
    if (compare > 0) {
      lo = mid + 1;
    } else if (compare < 0) {
      hi = mid - 1;
    } else {
      return &type_id;
    }
  }
  return nullptr;
}

const TypeId* DexFile::FindTypeId(dex::StringIndex string_idx) const {
  int32_t lo = 0;
  int32_t hi = NumTypeIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const TypeId& type_id = GetTypeId(dex::TypeIndex(mid));
    if (string_idx > type_id.descriptor_idx_) {
      lo = mid + 1;
    } else if (string_idx < type_id.descriptor_idx_) {
      hi = mid - 1;
    } else {
      return &type_id;
    }
  }
  return nullptr;
}

const ProtoId* DexFile::FindProtoId(dex::TypeIndex return_type_idx,
                                    const dex::TypeIndex* signature_type_idxs,
                                    uint32_t signature_length) const {
  int32_t lo = 0;
  int32_t hi = NumProtoIds() - 1;
  while (hi >= lo) {
    int32_t mid = (hi + lo) / 2;
    const dex::ProtoIndex proto_idx = static_cast<dex::ProtoIndex>(mid);
    const ProtoId& proto = GetProtoId(proto_idx);
    int compare = return_type_idx.index_ - proto.return_type_idx_.index_;
    if (compare == 0) {
      DexFileParameterIterator it(*this, proto);
      size_t i = 0;
      while (it.HasNext() && i < signature_length && compare == 0) {
        compare = signature_type_idxs[i].index_ - it.GetTypeIdx().index_;
        it.Next();
        i++;
      }
      if (compare == 0) {
        if (it.HasNext()) {
          compare = -1;
        } else if (i < signature_length) {
          compare = 1;
        }
      }
    }
    if (compare > 0) {
      lo = mid + 1;
    } else if (compare < 0) {
      hi = mid - 1;
    } else {
      return &proto;
    }
  }
  return nullptr;
}

// Given a signature place the type ids into the given vector
bool DexFile::CreateTypeList(std::string_view signature,
                             dex::TypeIndex* return_type_idx,
                             std::vector<dex::TypeIndex>* param_type_idxs) const {
  if (signature[0] != '(') {
    return false;
  }
  size_t offset = 1;
  size_t end = signature.size();
  bool process_return = false;
  while (offset < end) {
    size_t start_offset = offset;
    char c = signature[offset];
    offset++;
    if (c == ')') {
      process_return = true;
      continue;
    }
    while (c == '[') {  // process array prefix
      if (offset >= end) {  // expect some descriptor following [
        return false;
      }
      c = signature[offset];
      offset++;
    }
    if (c == 'L') {  // process type descriptors
      do {
        if (offset >= end) {  // unexpected early termination of descriptor
          return false;
        }
        c = signature[offset];
        offset++;
      } while (c != ';');
    }
    // TODO: avoid creating a std::string just to get a 0-terminated char array
    std::string descriptor(signature.data() + start_offset, offset - start_offset);
    const TypeId* type_id = FindTypeId(descriptor.c_str());
    if (type_id == nullptr) {
      return false;
    }
    dex::TypeIndex type_idx = GetIndexForTypeId(*type_id);
    if (!process_return) {
      param_type_idxs->push_back(type_idx);
    } else {
      *return_type_idx = type_idx;
      return offset == end;  // return true if the signature had reached a sensible end
    }
  }
  return false;  // failed to correctly parse return type
}

int32_t DexFile::FindTryItem(const TryItem* try_items, uint32_t tries_size, uint32_t address) {
  uint32_t min = 0;
  uint32_t max = tries_size;
  while (min < max) {
    const uint32_t mid = (min + max) / 2;

    const TryItem& ti = try_items[mid];
    const uint32_t start = ti.start_addr_;
    const uint32_t end = start + ti.insn_count_;

    if (address < start) {
      max = mid;
    } else if (address >= end) {
      min = mid + 1;
    } else {  // We have a winner!
      return mid;
    }
  }
  // No match.
  return -1;
}

// Read a signed integer.  "zwidth" is the zero-based byte count.
int32_t DexFile::ReadSignedInt(const uint8_t* ptr, int zwidth) {
  int32_t val = 0;
  for (int i = zwidth; i >= 0; --i) {
    val = ((uint32_t)val >> 8) | (((int32_t)*ptr++) << 24);
  }
  val >>= (3 - zwidth) * 8;
  return val;
}

// Read an unsigned integer.  "zwidth" is the zero-based byte count,
// "fill_on_right" indicates which side we want to zero-fill from.
uint32_t DexFile::ReadUnsignedInt(const uint8_t* ptr, int zwidth, bool fill_on_right) {
  uint32_t val = 0;
  for (int i = zwidth; i >= 0; --i) {
    val = (val >> 8) | (((uint32_t)*ptr++) << 24);
  }
  if (!fill_on_right) {
    val >>= (3 - zwidth) * 8;
  }
  return val;
}

// Read a signed long.  "zwidth" is the zero-based byte count.
int64_t DexFile::ReadSignedLong(const uint8_t* ptr, int zwidth) {
  int64_t val = 0;
  for (int i = zwidth; i >= 0; --i) {
    val = ((uint64_t)val >> 8) | (((int64_t)*ptr++) << 56);
  }
  val >>= (7 - zwidth) * 8;
  return val;
}

// Read an unsigned long.  "zwidth" is the zero-based byte count,
// "fill_on_right" indicates which side we want to zero-fill from.
uint64_t DexFile::ReadUnsignedLong(const uint8_t* ptr, int zwidth, bool fill_on_right) {
  uint64_t val = 0;
  for (int i = zwidth; i >= 0; --i) {
    val = (val >> 8) | (((uint64_t)*ptr++) << 56);
  }
  if (!fill_on_right) {
    val >>= (7 - zwidth) * 8;
  }
  return val;
}

void DexFile::AppendPrettyMethod(uint32_t method_idx,
                                 bool with_signature,
                                 std::string* const result) const {
  if (method_idx >= NumMethodIds()) {
    android::base::StringAppendF(result, "<<invalid-method-idx-%d>>", method_idx);
    return;
  }
  const MethodId& method_id = GetMethodId(method_idx);
  const ProtoId* proto_id = with_signature ? &GetProtoId(method_id.proto_idx_) : nullptr;
  if (with_signature) {
    AppendPrettyDescriptor(StringByTypeIdx(proto_id->return_type_idx_), result);
    result->push_back(' ');
  }
  AppendPrettyDescriptor(GetMethodDeclaringClassDescriptor(method_id), result);
  result->push_back('.');
  result->append(GetMethodName(method_id));
  if (with_signature) {
    result->push_back('(');
    const TypeList* params = GetProtoParameters(*proto_id);
    if (params != nullptr) {
      const char* separator = "";
      for (uint32_t i = 0u, size = params->Size(); i != size; ++i) {
        result->append(separator);
        separator = ", ";
        AppendPrettyDescriptor(StringByTypeIdx(params->GetTypeItem(i).type_idx_), result);
      }
    }
    result->push_back(')');
  }
}

std::string DexFile::PrettyField(uint32_t field_idx, bool with_type) const {
  if (field_idx >= NumFieldIds()) {
    return StringPrintf("<<invalid-field-idx-%d>>", field_idx);
  }
  const FieldId& field_id = GetFieldId(field_idx);
  std::string result;
  if (with_type) {
    result += GetFieldTypeDescriptor(field_id);
    result += ' ';
  }
  AppendPrettyDescriptor(GetFieldDeclaringClassDescriptor(field_id), &result);
  result += '.';
  result += GetFieldName(field_id);
  return result;
}

std::string DexFile::PrettyType(dex::TypeIndex type_idx) const {
  if (type_idx.index_ >= NumTypeIds()) {
    return StringPrintf("<<invalid-type-idx-%d>>", type_idx.index_);
  }
  const TypeId& type_id = GetTypeId(type_idx);
  return PrettyDescriptor(GetTypeDescriptor(type_id));
}

dex::ProtoIndex DexFile::GetProtoIndexForCallSite(uint32_t call_site_idx) const {
  const CallSiteIdItem& csi = GetCallSiteId(call_site_idx);
  CallSiteArrayValueIterator it(*this, csi);
  it.Next();
  it.Next();
  DCHECK_EQ(EncodedArrayValueIterator::ValueType::kMethodType, it.GetValueType());
  return dex::ProtoIndex(it.GetJavaValue().i);
}

// Checks that visibility is as expected. Includes special behavior for M and
// before to allow runtime and build visibility when expecting runtime.
std::ostream& operator<<(std::ostream& os, const DexFile& dex_file) {
  os << StringPrintf("[DexFile: %s dex-checksum=%08x location-checksum=%08x %p-%p]",
                     dex_file.GetLocation().c_str(),
                     dex_file.GetHeader().checksum_, dex_file.GetLocationChecksum(),
                     dex_file.Begin(), dex_file.Begin() + dex_file.Size());
  return os;
}

EncodedArrayValueIterator::EncodedArrayValueIterator(const DexFile& dex_file,
                                                     const uint8_t* array_data)
    : dex_file_(dex_file),
      array_size_(),
      pos_(-1),
      ptr_(array_data),
      type_(kByte) {
  array_size_ = (ptr_ != nullptr) ? DecodeUnsignedLeb128(&ptr_) : 0;
  if (array_size_ > 0) {
    bool ok [[maybe_unused]] = MaybeNext();
  }
}

bool EncodedArrayValueIterator::MaybeNext() {
  pos_++;
  if (pos_ >= array_size_) {
    type_ = kEndOfInput;
    return true;
  }
  uint8_t value_type = *ptr_++;
  uint8_t value_arg = value_type >> kEncodedValueArgShift;
  size_t width = value_arg + 1;  // assume and correct later
  type_ = static_cast<ValueType>(value_type & kEncodedValueTypeMask);
  switch (type_) {
  case kBoolean:
    jval_.i = (value_arg != 0) ? 1 : 0;
    width = 0;
    break;
  case kByte:
    jval_.i = DexFile::ReadSignedInt(ptr_, value_arg);
    CHECK(IsInt<8>(jval_.i));
    break;
  case kShort:
    jval_.i = DexFile::ReadSignedInt(ptr_, value_arg);
    CHECK(IsInt<16>(jval_.i));
    break;
  case kChar:
    jval_.i = DexFile::ReadUnsignedInt(ptr_, value_arg, false);
    CHECK(IsUint<16>(jval_.i));
    break;
  case kInt:
    jval_.i = DexFile::ReadSignedInt(ptr_, value_arg);
    break;
  case kLong:
    jval_.j = DexFile::ReadSignedLong(ptr_, value_arg);
    break;
  case kFloat:
    jval_.i = DexFile::ReadUnsignedInt(ptr_, value_arg, true);
    break;
  case kDouble:
    jval_.j = DexFile::ReadUnsignedLong(ptr_, value_arg, true);
    break;
  case kString:
  case kType:
  case kMethodType:
  case kMethodHandle:
    jval_.i = DexFile::ReadUnsignedInt(ptr_, value_arg, false);
    break;
  case kField:
  case kMethod:
  case kEnum:
  case kArray:
  case kAnnotation:
    return false;
  case kNull:
    jval_.l = nullptr;
    width = 0;
    break;
  default:
    return false;
  }
  ptr_ += width;
  return true;
}

namespace dex {

std::ostream& operator<<(std::ostream& os, const ProtoIndex& index) {
  os << "ProtoIndex[" << index.index_ << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const StringIndex& index) {
  os << "StringIndex[" << index.index_ << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const TypeIndex& index) {
  os << "TypeIndex[" << index.index_ << "]";
  return os;
}

}  // namespace dex

}  // namespace art
