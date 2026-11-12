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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_H_

#include <android-base/logging.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/array_ref.h"
#include "base/globals.h"
#include "base/macros.h"
#include "base/mman.h"  // For the PROT_* and MAP_* constants.
#include "base/value_object.h"
#include "dex_file_structs.h"
#include "dex_file_types.h"
#include "jni.h"
#include "modifiers.h"

namespace art {

class ClassDataItemIterator;
class ClassIterator;
class CompactDexFile;
class DexInstructionIterator;
enum InvokeType : uint32_t;
template <typename Iter> class IterationRange;
class MemMap;
class OatDexFile;
class Signature;
class StandardDexFile;
class ZipArchive;

namespace hiddenapi {
enum class Domain : char;
}  // namespace hiddenapi

// Owns the physical storage that backs one or more DexFiles (that is, it can be shared).
// It frees the storage (e.g. closes file) when all DexFiles that use it are all closed.
//
// The memory range must include all data used by the DexFiles including any shared data.
//
// It might also include surrounding non-dex data (e.g. it might represent vdex file).
class DexFileContainer {
 public:
  DexFileContainer() { }
  virtual ~DexFileContainer() {}

  virtual bool IsReadOnly() const = 0;

  // Make the underlying writeable. Return true on success (memory can be written).
  virtual bool EnableWrite() = 0;
  // Make the underlying read-only. Return true on success (memory is read-only now).
  virtual bool DisableWrite() = 0;

  virtual const uint8_t* Begin() const = 0;
  virtual const uint8_t* End() const = 0;
  size_t Size() const { return End() - Begin(); }

  // TODO: Remove. This is only used by dexlayout to override the data section of the dex header,
  //       and redirect it to intermediate memory buffer at completely unrelated memory location.
  virtual ArrayRef<const uint8_t> Data() const { return {}; }

  bool IsZip() const { return is_zip_; }
  void SetIsZip() { is_zip_ = true; }
  virtual bool IsFileMap() const { return false; }

 private:
  bool is_zip_ = false;
  DISALLOW_COPY_AND_ASSIGN(DexFileContainer);
};

class MemoryDexFileContainer : public DexFileContainer {
 public:
  MemoryDexFileContainer(const uint8_t* begin, const uint8_t* end) : begin_(begin), end_(end) {}
  MemoryDexFileContainer(const uint8_t* begin, size_t size) : begin_(begin), end_(begin + size) {}
  bool IsReadOnly() const override { return true; }
  bool EnableWrite() override { return false; }
  bool DisableWrite() override { return false; }
  const uint8_t* Begin() const override { return begin_; }
  const uint8_t* End() const override { return end_; }

 private:
  const uint8_t* const begin_;
  const uint8_t* const end_;
  DISALLOW_COPY_AND_ASSIGN(MemoryDexFileContainer);
};

// Dex file is the API that exposes native dex files (ordinary dex files) and CompactDex.
// Originally, the dex file format used by ART was mostly the same as APKs. The only change was
// quickened opcodes and layout optimizations.
// Since ART needs to support both native dex files and CompactDex files, the DexFile interface
// provides an abstraction to facilitate this.
class DexFile {
 public:
  // Number of bytes in the dex file magic.
  static constexpr size_t kDexMagicSize = 4;
  static constexpr size_t kDexVersionLen = 4;

  static constexpr uint32_t kDexContainerVersion = 41;

  // First Dex format version enforcing class definition ordering rules.
  static constexpr uint32_t kClassDefinitionOrderEnforcedVersion = 37;

  static constexpr size_t kSha1DigestSize = 20;
  static constexpr uint32_t kDexEndianConstant = 0x12345678;

  // The value of an invalid index.
  static constexpr uint16_t kDexNoIndex16 = 0xFFFF;
  static constexpr uint32_t kDexNoIndex32 = 0xFFFFFFFF;

  using Magic = std::array<uint8_t, 8>;

  struct Sha1 : public std::array<uint8_t, kSha1DigestSize> {
    std::string ToString() const;
  };

  static_assert(std::is_standard_layout_v<Sha1>);

  // Raw header_item.
  struct Header {
    Magic magic_ = {};
    uint32_t checksum_ = 0;  // See also location_checksum_
    Sha1 signature_ = {};
    uint32_t file_size_ = 0;  // size of entire file
    uint32_t header_size_ = 0;  // offset to start of next section
    uint32_t endian_tag_ = 0;
    uint32_t link_size_ = 0;  // unused
    uint32_t link_off_ = 0;  // unused
    uint32_t map_off_ = 0;  // map list offset from data_off_
    uint32_t string_ids_size_ = 0;  // number of StringIds
    uint32_t string_ids_off_ = 0;  // file offset of StringIds array
    uint32_t type_ids_size_ = 0;  // number of TypeIds, we don't support more than 65535
    uint32_t type_ids_off_ = 0;  // file offset of TypeIds array
    uint32_t proto_ids_size_ = 0;  // number of ProtoIds, we don't support more than 65535
    uint32_t proto_ids_off_ = 0;  // file offset of ProtoIds array
    uint32_t field_ids_size_ = 0;  // number of FieldIds
    uint32_t field_ids_off_ = 0;  // file offset of FieldIds array
    uint32_t method_ids_size_ = 0;  // number of MethodIds
    uint32_t method_ids_off_ = 0;  // file offset of MethodIds array
    uint32_t class_defs_size_ = 0;  // number of ClassDefs
    uint32_t class_defs_off_ = 0;  // file offset of ClassDef array
    uint32_t data_size_ = 0;  // size of data section
    uint32_t data_off_ = 0;  // file offset of data section

    // Decode the dex magic version
    uint32_t GetVersion() const;

    // Get the header_size that is expected for this version.
    uint32_t GetExpectedHeaderSize() const;

    // Returns true for standard DEX version 41 or newer.
    bool HasDexContainer() const;

    // Returns offset of this header within the container.
    // Returns 0 for older dex versions without container.
    uint32_t HeaderOffset() const;

    // Returns size of the whole container.
    // Returns file_size_ for older dex versions without container.
    uint32_t ContainerSize() const;

    // Set the DEX container fields to the given values.
    // Must be [0, file_size_) for older dex versions.
    void SetDexContainer(size_t header_offset, size_t container_size);
  };

  struct HeaderV41 : public Header {
    uint32_t container_size_ = 0;  // total size of all dex files in the container.
    uint32_t header_offset_ = 0;   // offset of this dex's header in the container.
  };

  // Map item type codes.
  enum MapItemType : uint16_t {  // private
    kDexTypeHeaderItem               = 0x0000,
    kDexTypeStringIdItem             = 0x0001,
    kDexTypeTypeIdItem               = 0x0002,
    kDexTypeProtoIdItem              = 0x0003,
    kDexTypeFieldIdItem              = 0x0004,
    kDexTypeMethodIdItem             = 0x0005,
    kDexTypeClassDefItem             = 0x0006,
    kDexTypeCallSiteIdItem           = 0x0007,
    kDexTypeMethodHandleItem         = 0x0008,
    kDexTypeMapList                  = 0x1000,
    kDexTypeTypeList                 = 0x1001,
    kDexTypeAnnotationSetRefList     = 0x1002,
    kDexTypeAnnotationSetItem        = 0x1003,
    kDexTypeClassDataItem            = 0x2000,
    kDexTypeCodeItem                 = 0x2001,
    kDexTypeStringDataItem           = 0x2002,
    kDexTypeDebugInfoItem            = 0x2003,
    kDexTypeAnnotationItem           = 0x2004,
    kDexTypeEncodedArrayItem         = 0x2005,
    kDexTypeAnnotationsDirectoryItem = 0x2006,
    kDexTypeHiddenapiClassData       = 0xF000,
  };

  // MethodHandle Types
  enum class MethodHandleType : uint16_t {  // private
    kStaticPut         = 0x0000,  // a setter for a given static field.
    kStaticGet         = 0x0001,  // a getter for a given static field.
    kInstancePut       = 0x0002,  // a setter for a given instance field.
    kInstanceGet       = 0x0003,  // a getter for a given instance field.
    kInvokeStatic      = 0x0004,  // an invoker for a given static method.
    kInvokeInstance    = 0x0005,  // invoke_instance : an invoker for a given instance method. This
                                  // can be any non-static method on any class (or interface) except
                                  // for “<init>”.
    kInvokeConstructor = 0x0006,  // an invoker for a given constructor.
    kInvokeDirect      = 0x0007,  // an invoker for a direct (special) method.
    kInvokeInterface   = 0x0008,  // an invoker for an interface method.
    kLast = kInvokeInterface
  };

  // Annotation constants.
  enum {
    kDexVisibilityBuild         = 0x00,     /* annotation visibility */
    kDexVisibilityRuntime       = 0x01,
    kDexVisibilitySystem        = 0x02,

    kDexAnnotationByte          = 0x00,
    kDexAnnotationShort         = 0x02,
    kDexAnnotationChar          = 0x03,
    kDexAnnotationInt           = 0x04,
    kDexAnnotationLong          = 0x06,
    kDexAnnotationFloat         = 0x10,
    kDexAnnotationDouble        = 0x11,
    kDexAnnotationMethodType    = 0x15,
    kDexAnnotationMethodHandle  = 0x16,
    kDexAnnotationString        = 0x17,
    kDexAnnotationType          = 0x18,
    kDexAnnotationField         = 0x19,
    kDexAnnotationMethod        = 0x1a,
    kDexAnnotationEnum          = 0x1b,
    kDexAnnotationArray         = 0x1c,
    kDexAnnotationAnnotation    = 0x1d,
    kDexAnnotationNull          = 0x1e,
    kDexAnnotationBoolean       = 0x1f,

    kDexAnnotationValueTypeMask = 0x1f,     /* low 5 bits */
    kDexAnnotationValueArgShift = 5,
  };

  enum AnnotationResultStyle {  // private
    kAllObjects,
    kPrimitivesOrObjects,
    kAllRaw
  };

  struct AnnotationValue;

  // Closes a .dex file.
  virtual ~DexFile();

  const std::string& GetLocation() const {
    return location_;
  }

  // For DexFiles directly from .dex files, this is the checksum from the DexFile::Header.
  // For DexFiles opened from a zip files, this will be the ZipEntry CRC32 of classes.dex.
  uint32_t GetLocationChecksum() const {
    return location_checksum_;
  }

  Sha1 GetSha1() const { return header_->signature_; }

  const Header& GetHeader() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return *header_;
  }

  // Decode the dex magic version
  uint32_t GetDexVersion() const {
    return GetHeader().GetVersion();
  }

  // Returns true if this is DEX V41 or later (i.e. supports container).
  // Returns true even if the container contains just a single DEX file.
  bool HasDexContainer() const { return GetHeader().HasDexContainer(); }

  // Returns the whole memory range of the DEX V41 container.
  // Returns just the range of the DEX file for V40 or older.
  ArrayRef<const uint8_t> GetDexContainerRange() const {
    return {Begin() - header_->HeaderOffset(), header_->ContainerSize()};
  }

  bool IsDexContainerFirstEntry() const { return Begin() == GetDexContainerRange().begin(); }

  bool IsDexContainerLastEntry() const { return End() == GetDexContainerRange().end(); }

  // Returns true if the byte string points to the magic value.
  virtual bool IsMagicValid() const = 0;

  // Returns true if the byte string after the magic is the correct value.
  virtual bool IsVersionValid() const = 0;

  // Returns true if the dex file supports default methods.
  virtual bool SupportsDefaultMethods() const = 0;

  // Returns the maximum size in bytes needed to store an equivalent dex file strictly conforming to
  // the dex file specification. That is the size if we wanted to get rid of all the
  // quickening/compact-dexing/etc.
  //
  // TODO This should really be an exact size! b/72402467
  virtual size_t GetDequickenedSize() const = 0;

  // Returns the number of string identifiers in the .dex file.
  size_t NumStringIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->string_ids_size_;
  }

  // Returns the StringId at the specified index.
  const dex::StringId& GetStringId(dex::StringIndex idx) const {
    DCHECK_LT(idx.index_, NumStringIds()) << GetLocation();
    return string_ids_[idx.index_];
  }

  dex::StringIndex GetIndexForStringId(const dex::StringId& string_id) const {
    CHECK_GE(&string_id, string_ids_) << GetLocation();
    CHECK_LT(&string_id, string_ids_ + header_->string_ids_size_) << GetLocation();
    return dex::StringIndex(&string_id - string_ids_);
  }

  int32_t GetStringLength(const dex::StringId& string_id) const;

  // Returns a pointer to the UTF-8 string data referred to by the given string_id as well as the
  // length of the string when decoded as a UTF-16 string. Note the UTF-16 length is not the same
  // as the string length of the string data.
  const char* GetStringDataAndUtf16Length(const dex::StringId& string_id,
                                          uint32_t* utf16_length) const;

  const char* GetStringData(const dex::StringId& string_id) const;

  // Index version of GetStringDataAndUtf16Length.
  const char* StringDataAndUtf16LengthByIdx(dex::StringIndex idx, uint32_t* utf16_length) const;

  const char* StringDataByIdx(dex::StringIndex idx) const;
  std::string_view StringViewByIdx(dex::StringIndex idx) const;

  // Looks up a string id for a given modified utf8 string.
  const dex::StringId* FindStringId(const char* string) const;

  const dex::TypeId* FindTypeId(const char* string) const;
  const dex::TypeId* FindTypeId(std::string_view string) const {
    return FindTypeId(std::string(string).c_str());
  }

  // Returns the number of type identifiers in the .dex file.
  uint32_t NumTypeIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->type_ids_size_;
  }

  bool IsTypeIndexValid(dex::TypeIndex idx) const {
    return idx.IsValid() && idx.index_ < NumTypeIds();
  }

  // Returns the TypeId at the specified index.
  const dex::TypeId& GetTypeId(dex::TypeIndex idx) const {
    DCHECK_LT(idx.index_, NumTypeIds()) << GetLocation();
    return type_ids_[idx.index_];
  }

  dex::TypeIndex GetIndexForTypeId(const dex::TypeId& type_id) const {
    CHECK_GE(&type_id, type_ids_) << GetLocation();
    CHECK_LT(&type_id, type_ids_ + header_->type_ids_size_) << GetLocation();
    size_t result = &type_id - type_ids_;
    DCHECK_LT(result, 65536U) << GetLocation();
    return dex::TypeIndex(static_cast<uint16_t>(result));
  }

  // Get the descriptor string associated with a given type index.
  const char* StringByTypeIdx(dex::TypeIndex idx, uint32_t* unicode_length) const;

  const char* StringByTypeIdx(dex::TypeIndex idx) const;

  // Returns the type descriptor string of a type id.
  const char* GetTypeDescriptor(const dex::TypeId& type_id) const;
  std::string_view GetTypeDescriptorView(const dex::TypeId& type_id) const;

  // Looks up a type for the given string index
  const dex::TypeId* FindTypeId(dex::StringIndex string_idx) const;

  // Returns the number of field identifiers in the .dex file.
  size_t NumFieldIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->field_ids_size_;
  }

  // Returns the FieldId at the specified index.
  const dex::FieldId& GetFieldId(uint32_t idx) const {
    DCHECK_LT(idx, NumFieldIds()) << GetLocation();
    return field_ids_[idx];
  }

  uint32_t GetIndexForFieldId(const dex::FieldId& field_id) const {
    CHECK_GE(&field_id, field_ids_) << GetLocation();
    CHECK_LT(&field_id, field_ids_ + header_->field_ids_size_) << GetLocation();
    return &field_id - field_ids_;
  }

  // Looks up a field by its declaring class, name and type
  const dex::FieldId* FindFieldId(const dex::TypeId& declaring_klass,
                                  const dex::StringId& name,
                                  const dex::TypeId& type) const;

  // Return the code-item offset associated with the class and method or nullopt
  // if the method does not exist or has no code.
  std::optional<uint32_t> GetCodeItemOffset(const dex::ClassDef& class_def,
                                            uint32_t dex_method_idx) const;

  // Return the code-item offset associated with the class and method or
  // LOG(FATAL) if the method does not exist or has no code.
  uint32_t FindCodeItemOffset(const dex::ClassDef& class_def,
                              uint32_t dex_method_idx) const;

  virtual uint32_t GetCodeItemSize(const dex::CodeItem& disk_code_item) const = 0;

  // Returns the declaring class descriptor string of a field id.
  const char* GetFieldDeclaringClassDescriptor(const dex::FieldId& field_id) const {
    const dex::TypeId& type_id = GetTypeId(field_id.class_idx_);
    return GetTypeDescriptor(type_id);
  }

  // Returns the class descriptor string of a field id.
  const char* GetFieldTypeDescriptor(const dex::FieldId& field_id) const;
  std::string_view GetFieldTypeDescriptorView(const dex::FieldId& field_id) const;

  // Returns the name of a field id.
  const char* GetFieldName(const dex::FieldId& field_id) const;
  std::string_view GetFieldNameView(const dex::FieldId& field_id) const;

  // Returns the number of method identifiers in the .dex file.
  size_t NumMethodIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->method_ids_size_;
  }

  // Returns the MethodId at the specified index.
  const dex::MethodId& GetMethodId(uint32_t idx) const {
    DCHECK_LT(idx, NumMethodIds()) << GetLocation();
    return method_ids_[idx];
  }

  uint32_t GetIndexForMethodId(const dex::MethodId& method_id) const {
    CHECK_GE(&method_id, method_ids_) << GetLocation();
    CHECK_LT(&method_id, method_ids_ + header_->method_ids_size_) << GetLocation();
    return &method_id - method_ids_;
  }

  // Looks up a method by its declaring class, name and proto_id
  const dex::MethodId* FindMethodId(const dex::TypeId& declaring_klass,
                                    const dex::StringId& name,
                                    const dex::ProtoId& signature) const;

  const dex::MethodId* FindMethodIdByIndex(dex::TypeIndex declaring_klass,
                                           dex::StringIndex name,
                                           dex::ProtoIndex signature) const;

  // Returns the declaring class descriptor string of a method id.
  const char* GetMethodDeclaringClassDescriptor(const dex::MethodId& method_id) const;

  // Returns the prototype of a method id.
  const dex::ProtoId& GetMethodPrototype(const dex::MethodId& method_id) const {
    return GetProtoId(method_id.proto_idx_);
  }

  // Returns a representation of the signature of a method id.
  const Signature GetMethodSignature(const dex::MethodId& method_id) const;

  // Returns a representation of the signature of a proto id.
  const Signature GetProtoSignature(const dex::ProtoId& proto_id) const;

  // Returns the name of a method id.
  const char* GetMethodName(const dex::MethodId& method_id) const;
  const char* GetMethodName(const dex::MethodId& method_id, uint32_t* utf_length) const;
  const char* GetMethodName(uint32_t idx) const;
  const char* GetMethodName(uint32_t idx, uint32_t* utf_length) const;
  std::string_view GetMethodNameView(const dex::MethodId& method_id) const;
  std::string_view GetMethodNameView(uint32_t idx) const;

  // Returns the shorty of a method by its index.
  const char* GetMethodShorty(uint32_t idx) const;

  // Returns the shorty of a method id.
  const char* GetMethodShorty(const dex::MethodId& method_id) const;
  const char* GetMethodShorty(const dex::MethodId& method_id, uint32_t* length) const;
  std::string_view GetMethodShortyView(const dex::MethodId& method_id) const;

  // Returns the number of class definitions in the .dex file.
  uint32_t NumClassDefs() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->class_defs_size_;
  }

  // Returns the ClassDef at the specified index.
  const dex::ClassDef& GetClassDef(uint16_t idx) const {
    DCHECK_LT(idx, NumClassDefs()) << GetLocation();
    return class_defs_[idx];
  }

  uint16_t GetIndexForClassDef(const dex::ClassDef& class_def) const {
    CHECK_GE(&class_def, class_defs_) << GetLocation();
    CHECK_LT(&class_def, class_defs_ + header_->class_defs_size_) << GetLocation();
    return &class_def - class_defs_;
  }

  // Returns the class descriptor string of a class definition.
  const char* GetClassDescriptor(const dex::ClassDef& class_def) const;

  // Looks up a class definition by its type index.
  const dex::ClassDef* FindClassDef(dex::TypeIndex type_idx) const;

  const dex::TypeList* GetInterfacesList(const dex::ClassDef& class_def) const {
    return DataPointer<dex::TypeList>(class_def.interfaces_off_);
  }

  uint32_t NumMethodHandles() const {
    return num_method_handles_;
  }

  const dex::MethodHandleItem& GetMethodHandle(uint32_t idx) const {
    CHECK_LT(idx, NumMethodHandles());
    return method_handles_[idx];
  }

  uint32_t NumCallSiteIds() const {
    return num_call_site_ids_;
  }

  const dex::CallSiteIdItem& GetCallSiteId(uint32_t idx) const {
    CHECK_LT(idx, NumCallSiteIds());
    return call_site_ids_[idx];
  }

  // Returns a pointer to the raw memory mapped class_data_item
  const uint8_t* GetClassData(const dex::ClassDef& class_def) const {
    return DataPointer<uint8_t>(class_def.class_data_off_);
  }

  // Return the code item for a provided offset.
  const dex::CodeItem* GetCodeItem(const uint32_t code_off) const {
    // May be null for native or abstract methods.
    return DataPointer<dex::CodeItem>(code_off);
  }

  const char* GetReturnTypeDescriptor(const dex::ProtoId& proto_id) const;

  // Returns the number of prototype identifiers in the .dex file.
  size_t NumProtoIds() const {
    DCHECK(header_ != nullptr) << GetLocation();
    return header_->proto_ids_size_;
  }

  // Returns the ProtoId at the specified index.
  const dex::ProtoId& GetProtoId(dex::ProtoIndex idx) const {
    DCHECK_LT(idx.index_, NumProtoIds()) << GetLocation();
    return proto_ids_[idx.index_];
  }

  dex::ProtoIndex GetIndexForProtoId(const dex::ProtoId& proto_id) const {
    CHECK_GE(&proto_id, proto_ids_) << GetLocation();
    CHECK_LT(&proto_id, proto_ids_ + header_->proto_ids_size_) << GetLocation();
    return dex::ProtoIndex(&proto_id - proto_ids_);
  }

  // Looks up a proto id for a given return type and signature type list
  const dex::ProtoId* FindProtoId(dex::TypeIndex return_type_idx,
                                  const dex::TypeIndex* signature_type_idxs,
                             uint32_t signature_length) const;
  const dex::ProtoId* FindProtoId(dex::TypeIndex return_type_idx,
                                  const std::vector<dex::TypeIndex>& signature_type_idxs) const {
    return FindProtoId(return_type_idx, &signature_type_idxs[0], signature_type_idxs.size());
  }

  // Given a signature place the type ids into the given vector, returns true on success
  bool CreateTypeList(std::string_view signature,
                      dex::TypeIndex* return_type_idx,
                      std::vector<dex::TypeIndex>* param_type_idxs) const;

  // Returns the short form method descriptor for the given prototype.
  const char* GetShorty(dex::ProtoIndex proto_idx) const;
  std::string_view GetShortyView(dex::ProtoIndex proto_idx) const;
  std::string_view GetShortyView(const dex::ProtoId& proto_id) const;

  const dex::TypeList* GetProtoParameters(const dex::ProtoId& proto_id) const {
    return DataPointer<dex::TypeList>(proto_id.parameters_off_);
  }

  const uint8_t* GetEncodedStaticFieldValuesArray(const dex::ClassDef& class_def) const {
    return DataPointer<uint8_t>(class_def.static_values_off_);
  }

  const uint8_t* GetCallSiteEncodedValuesArray(const dex::CallSiteIdItem& call_site_id) const {
    return DataBegin() + call_site_id.data_off_;
  }

  dex::ProtoIndex GetProtoIndexForCallSite(uint32_t call_site_idx) const;

  static const dex::TryItem* GetTryItems(const DexInstructionIterator& code_item_end,
                                         uint32_t offset);

  // Get the base of the encoded data for the given DexCode.
  static const uint8_t* GetCatchHandlerData(const DexInstructionIterator& code_item_end,
                                            uint32_t tries_size,
                                            uint32_t offset);

  // Find which try region is associated with the given address (ie dex pc). Returns -1 if none.
  static int32_t FindTryItem(const dex::TryItem* try_items, uint32_t tries_size, uint32_t address);

  // Get the pointer to the start of the debugging data
  const uint8_t* GetDebugInfoStream(uint32_t debug_info_off) const {
    // Check that the offset is in bounds.
    // Note that although the specification says that 0 should be used if there
    // is no debug information, some applications incorrectly use 0xFFFFFFFF.
    return (debug_info_off == 0 || debug_info_off >= DataSize()) ? nullptr :
                                                                   DataBegin() + debug_info_off;
  }

  struct PositionInfo {
    PositionInfo() = default;

    uint32_t address_ = 0;  // In 16-bit code units.
    uint32_t line_ = 0;  // Source code line number starting at 1.
    const char* source_file_ = nullptr;  // nullptr if the file from ClassDef still applies.
    bool prologue_end_ = false;
    bool epilogue_begin_ = false;
  };

  struct LocalInfo {
    LocalInfo() = default;

    const char* name_ = nullptr;  // E.g., list.  It can be nullptr if unknown.
    const char* descriptor_ = nullptr;  // E.g., Ljava/util/LinkedList;
    const char* signature_ = nullptr;  // E.g., java.util.LinkedList<java.lang.Integer>
    uint32_t start_address_ = 0;  // PC location where the local is first defined.
    uint32_t end_address_ = 0;  // PC location where the local is no longer defined.
    uint16_t reg_ = 0;  // Dex register which stores the values.
    bool is_live_ = false;  // Is the local defined and live.
  };

  // Callback for "new locals table entry".
  using DexDebugNewLocalCb = void (*)(void* context, const LocalInfo& entry);

  const dex::AnnotationsDirectoryItem* GetAnnotationsDirectory(const dex::ClassDef& class_def)
      const {
    return DataPointer<dex::AnnotationsDirectoryItem>(class_def.annotations_off_);
  }

  const dex::AnnotationSetItem* GetClassAnnotationSet(const dex::AnnotationsDirectoryItem* anno_dir)
      const {
    return DataPointer<dex::AnnotationSetItem>(anno_dir->class_annotations_off_);
  }

  const dex::FieldAnnotationsItem* GetFieldAnnotations(
      const dex::AnnotationsDirectoryItem* anno_dir) const {
    return (anno_dir->fields_size_ == 0)
         ? nullptr
         : reinterpret_cast<const dex::FieldAnnotationsItem*>(&anno_dir[1]);
  }

  const dex::MethodAnnotationsItem* GetMethodAnnotations(
      const dex::AnnotationsDirectoryItem* anno_dir) const {
    if (anno_dir->methods_size_ == 0) {
      return nullptr;
    }
    // Skip past the header and field annotations.
    const uint8_t* addr = reinterpret_cast<const uint8_t*>(&anno_dir[1]);
    addr += anno_dir->fields_size_ * sizeof(dex::FieldAnnotationsItem);
    return reinterpret_cast<const dex::MethodAnnotationsItem*>(addr);
  }

  const dex::ParameterAnnotationsItem* GetParameterAnnotations(
      const dex::AnnotationsDirectoryItem* anno_dir) const {
    if (anno_dir->parameters_size_ == 0) {
      return nullptr;
    }
    // Skip past the header, field annotations, and method annotations.
    const uint8_t* addr = reinterpret_cast<const uint8_t*>(&anno_dir[1]);
    addr += anno_dir->fields_size_ * sizeof(dex::FieldAnnotationsItem);
    addr += anno_dir->methods_size_ * sizeof(dex::MethodAnnotationsItem);
    return reinterpret_cast<const dex::ParameterAnnotationsItem*>(addr);
  }

  const dex::AnnotationSetItem* GetFieldAnnotationSetItem(
      const dex::FieldAnnotationsItem& anno_item) const {
    return DataPointer<dex::AnnotationSetItem>(anno_item.annotations_off_);
  }

  const dex::AnnotationSetItem* GetMethodAnnotationSetItem(
      const dex::MethodAnnotationsItem& anno_item) const {
    return DataPointer<dex::AnnotationSetItem>(anno_item.annotations_off_);
  }

  const dex::AnnotationSetRefList* GetParameterAnnotationSetRefList(
      const dex::ParameterAnnotationsItem* anno_item) const {
    return DataPointer<dex::AnnotationSetRefList>(anno_item->annotations_off_);
  }

  ALWAYS_INLINE const dex::AnnotationItem* GetAnnotationItemAtOffset(uint32_t offset) const {
    return DataPointer<dex::AnnotationItem>(offset);
  }

  ALWAYS_INLINE const dex::HiddenapiClassData* GetHiddenapiClassDataAtOffset(uint32_t offset)
      const {
    return DataPointer<dex::HiddenapiClassData>(offset);
  }

  ALWAYS_INLINE const dex::HiddenapiClassData* GetHiddenapiClassData() const {
    return hiddenapi_class_data_;
  }

  ALWAYS_INLINE bool HasHiddenapiClassData() const {
    return hiddenapi_class_data_ != nullptr;
  }

  const dex::AnnotationItem* GetAnnotationItem(const dex::AnnotationSetItem* set_item,
                                               uint32_t index) const {
    DCHECK_LE(index, set_item->size_);
    return GetAnnotationItemAtOffset(set_item->entries_[index]);
  }

  const dex::AnnotationSetItem* GetSetRefItemItem(const dex::AnnotationSetRefItem* anno_item)
      const {
    return DataPointer<dex::AnnotationSetItem>(anno_item->annotations_off_);
  }

  // Debug info opcodes and constants
  enum {
    DBG_END_SEQUENCE         = 0x00,
    DBG_ADVANCE_PC           = 0x01,
    DBG_ADVANCE_LINE         = 0x02,
    DBG_START_LOCAL          = 0x03,
    DBG_START_LOCAL_EXTENDED = 0x04,
    DBG_END_LOCAL            = 0x05,
    DBG_RESTART_LOCAL        = 0x06,
    DBG_SET_PROLOGUE_END     = 0x07,
    DBG_SET_EPILOGUE_BEGIN   = 0x08,
    DBG_SET_FILE             = 0x09,
    DBG_FIRST_SPECIAL        = 0x0a,
    DBG_LINE_BASE            = -4,
    DBG_LINE_RANGE           = 15,
  };

  // Returns false if there is no debugging information or if it cannot be decoded.
  template<typename NewLocalCallback, typename IndexToStringData, typename TypeIndexToStringData>
  static bool DecodeDebugLocalInfo(const uint8_t* stream,
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
                                   const NewLocalCallback& new_local) NO_THREAD_SAFETY_ANALYSIS;
  template<typename NewLocalCallback>
  bool DecodeDebugLocalInfo(uint32_t registers_size,
                            uint32_t ins_size,
                            uint32_t insns_size_in_code_units,
                            uint32_t debug_info_offset,
                            bool is_static,
                            uint32_t method_idx,
                            const NewLocalCallback& new_local) const;

  // Returns false if there is no debugging information or if it cannot be decoded.
  template<typename DexDebugNewPosition, typename IndexToStringData>
  static bool DecodeDebugPositionInfo(const uint8_t* stream,
                                      const IndexToStringData& index_to_string_data,
                                      const DexDebugNewPosition& position_functor);

  const char* GetSourceFile(const dex::ClassDef& class_def) const {
    if (!class_def.source_file_idx_.IsValid()) {
      return nullptr;
    } else {
      return StringDataByIdx(class_def.source_file_idx_);
    }
  }

  bool IsReadOnly() const;

  bool EnableWrite() const;

  bool DisableWrite() const;

  const uint8_t* Begin() const { return begin_; }

  const uint8_t* End() const { return Begin() + Size(); }

  size_t Size() const { return header_->file_size_; }

  size_t SizeIncludingSharedData() const { return GetDexContainerRange().end() - Begin(); }

  static ArrayRef<const uint8_t> GetDataRange(const uint8_t* data, DexFileContainer* container);

  const uint8_t* DataBegin() const { return data_.data(); }

  size_t DataSize() const { return data_.size(); }

  template <typename T>
  const T* DataPointer(size_t offset) const {
    DCHECK_LT(offset, DataSize()) << "Offset past end of data section";
    return (offset != 0u) ? reinterpret_cast<const T*>(DataBegin() + offset) : nullptr;
  }

  const OatDexFile* GetOatDexFile() const {
    return oat_dex_file_;
  }

  // Used by oat writer.
  void SetOatDexFile(const OatDexFile* oat_dex_file) const {
    oat_dex_file_ = oat_dex_file;
  }

  // Read MapItems and validate/set remaining offsets.
  const dex::MapList* GetMapList() const {
    return reinterpret_cast<const dex::MapList*>(DataBegin() + header_->map_off_);
  }

  // Utility methods for reading integral values from a buffer.
  static int32_t ReadSignedInt(const uint8_t* ptr, int zwidth);
  static uint32_t ReadUnsignedInt(const uint8_t* ptr, int zwidth, bool fill_on_right);
  static int64_t ReadSignedLong(const uint8_t* ptr, int zwidth);
  static uint64_t ReadUnsignedLong(const uint8_t* ptr, int zwidth, bool fill_on_right);

  // Recalculates the checksum of the dex file. Does not use the current value in the header.
  virtual uint32_t CalculateChecksum() const;
  static uint32_t CalculateChecksum(const uint8_t* begin, size_t size);
  static uint32_t ChecksumMemoryRange(const uint8_t* begin, size_t size);

  // Number of bytes at the beginning of the dex file header which are skipped
  // when computing the adler32 checksum of the entire file.
  static constexpr uint32_t kNumNonChecksumBytes = OFFSETOF_MEMBER(DexFile::Header, signature_);

  // Appends a human-readable form of the method at an index.
  void AppendPrettyMethod(uint32_t method_idx, bool with_signature, std::string* result) const;
  // Returns a human-readable form of the field at an index.
  std::string PrettyField(uint32_t field_idx, bool with_type = true) const;
  // Returns a human-readable form of the type at an index.
  std::string PrettyType(dex::TypeIndex type_idx) const;

  ALWAYS_INLINE std::string PrettyMethod(uint32_t method_idx, bool with_signature = true) const {
    std::string result;
    AppendPrettyMethod(method_idx, with_signature, &result);
    return result;
  }

  // Not virtual for performance reasons.
  ALWAYS_INLINE bool IsCompactDexFile() const {
    return is_compact_dex_;
  }
  ALWAYS_INLINE bool IsStandardDexFile() const {
    return !is_compact_dex_;
  }
  ALWAYS_INLINE const StandardDexFile* AsStandardDexFile() const;
  ALWAYS_INLINE const CompactDexFile* AsCompactDexFile() const;

  hiddenapi::Domain GetHiddenapiDomain() const { return hiddenapi_domain_; }
  void SetHiddenapiDomain(hiddenapi::Domain value) const { hiddenapi_domain_ = value; }

  bool IsInMainSection(const void* addr) const {
    return Begin() <= addr && addr < Begin() + Size();
  }

  bool IsInDataSection(const void* addr) const {
    return DataBegin() <= addr && addr < DataBegin() + DataSize();
  }

  const std::shared_ptr<DexFileContainer>& GetContainer() const { return container_; }

  IterationRange<ClassIterator> GetClasses() const;

  template <typename Visitor>
  static uint32_t DecodeDebugInfoParameterNames(const uint8_t** debug_info,
                                                const Visitor& visitor);

  static inline bool StringEquals(const DexFile* df1, dex::StringIndex sidx1,
                                  const DexFile* df2, dex::StringIndex sidx2);

 protected:
  // First Dex format version supporting default methods.
  static constexpr uint32_t kDefaultMethodsVersion = 37;

  DexFile(const uint8_t* base,
          const std::string& location,
          uint32_t location_checksum,
          const OatDexFile* oat_dex_file,
          // Shared since several dex files may be stored in the same logical container.
          std::shared_ptr<DexFileContainer> container,
          bool is_compact_dex);

  template <typename T>
  const T* GetSection(const uint32_t* offset, DexFileContainer* container);

  // Top-level initializer that calls other Init methods.
  bool Init(std::string* error_msg);

  // Returns true if the header magic and version numbers are of the expected values.
  bool CheckMagicAndVersion(std::string* error_msg) const;

  // Initialize section info for sections only found in map. Returns true on success.
  void InitializeSectionsFromMapList();

  // The base address of the memory mapping.
  const uint8_t* const begin_;

  size_t unused_size_ = 0;  // Preserve layout for DRM (b/305203031).

  // Data memory range: Most dex offsets are relative to this memory range.
  // Standard dex: same as (begin_, size_).
  // Dex container: all dex files (starting from the first header).
  // Compact: shared data which is located after all non-shared data.
  //
  // This is different to the "data section" in the standard dex header.
  ArrayRef<const uint8_t> const data_;

  // Typically the dex file name when available, alternatively some identifying string.
  //
  // The ClassLinker will use this to match DexFiles the boot class
  // path to DexCache::GetLocation when loading from an image.
  const std::string location_;

  const uint32_t location_checksum_;

  // Points to the header section.
  const Header* const header_;

  // Points to the base of the string identifier list.
  const dex::StringId* const string_ids_;

  // Points to the base of the type identifier list.
  const dex::TypeId* const type_ids_;

  // Points to the base of the field identifier list.
  const dex::FieldId* const field_ids_;

  // Points to the base of the method identifier list.
  const dex::MethodId* const method_ids_;

  // Points to the base of the prototype identifier list.
  const dex::ProtoId* const proto_ids_;

  // Points to the base of the class definition list.
  const dex::ClassDef* const class_defs_;

  // Points to the base of the method handles list.
  const dex::MethodHandleItem* method_handles_;

  // Number of elements in the method handles list.
  size_t num_method_handles_;

  // Points to the base of the call sites id list.
  const dex::CallSiteIdItem* call_site_ids_;

  // Number of elements in the call sites list.
  size_t num_call_site_ids_;

  // Points to the base of the hiddenapi class data item_, or nullptr if the dex
  // file does not have one.
  const dex::HiddenapiClassData* hiddenapi_class_data_;

  // If this dex file was loaded from an oat file, oat_dex_file_ contains a
  // pointer to the OatDexFile it was loaded from. Otherwise oat_dex_file_ is
  // null.
  mutable const OatDexFile* oat_dex_file_;

  // Manages the underlying memory allocation.
  std::shared_ptr<DexFileContainer> container_;

  // If the dex file is a compact dex file. If false then the dex file is a standard dex file.
  const bool is_compact_dex_;

  // The domain this dex file belongs to for hidden API access checks.
  // It is decleared `mutable` because the domain is assigned after the DexFile
  // has been created and can be changed later by the runtime.
  mutable hiddenapi::Domain hiddenapi_domain_;

  friend class DexFileLoader;
  friend class DexFileVerifierTest;
  friend class OatWriter;
};

std::ostream& operator<<(std::ostream& os, const DexFile& dex_file);

// Iterate over a dex file's ProtoId's paramters
class DexFileParameterIterator {
 public:
  DexFileParameterIterator(const DexFile& dex_file, const dex::ProtoId& proto_id)
      : dex_file_(dex_file) {
    type_list_ = dex_file_.GetProtoParameters(proto_id);
    if (type_list_ != nullptr) {
      size_ = type_list_->Size();
    }
  }
  bool HasNext() const { return pos_ < size_; }
  size_t Size() const { return size_; }
  void Next() { ++pos_; }
  dex::TypeIndex GetTypeIdx() {
    return type_list_->GetTypeItem(pos_).type_idx_;
  }
  const char* GetDescriptor() {
    return dex_file_.StringByTypeIdx(dex::TypeIndex(GetTypeIdx()));
  }
 private:
  const DexFile& dex_file_;
  const dex::TypeList* type_list_ = nullptr;
  uint32_t size_ = 0;
  uint32_t pos_ = 0;
  DISALLOW_IMPLICIT_CONSTRUCTORS(DexFileParameterIterator);
};

class EncodedArrayValueIterator {
 public:
  EncodedArrayValueIterator(const DexFile& dex_file, const uint8_t* array_data);

  bool HasNext() const { return pos_ < array_size_; }

  WARN_UNUSED bool MaybeNext();

  ALWAYS_INLINE void Next() {
    bool ok = MaybeNext();
    DCHECK(ok) << "Unknown type: " << GetValueType();
  }

  enum ValueType {
    kByte         = 0x00,
    kShort        = 0x02,
    kChar         = 0x03,
    kInt          = 0x04,
    kLong         = 0x06,
    kFloat        = 0x10,
    kDouble       = 0x11,
    kMethodType   = 0x15,
    kMethodHandle = 0x16,
    kString       = 0x17,
    kType         = 0x18,
    kField        = 0x19,
    kMethod       = 0x1a,
    kEnum         = 0x1b,
    kArray        = 0x1c,
    kAnnotation   = 0x1d,
    kNull         = 0x1e,
    kBoolean      = 0x1f,
    kEndOfInput   = 0xff,
  };

  ValueType GetValueType() const { return type_; }
  const jvalue& GetJavaValue() const { return jval_; }

 protected:
  static constexpr uint8_t kEncodedValueTypeMask = 0x1f;  // 0b11111
  static constexpr uint8_t kEncodedValueArgShift = 5;

  const DexFile& dex_file_;
  size_t array_size_;  // Size of array.
  size_t pos_;  // Current position.
  const uint8_t* ptr_;  // Pointer into encoded data array.
  ValueType type_;  // Type of current encoded value.
  jvalue jval_;  // Value of current encoded value.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodedArrayValueIterator);
};
std::ostream& operator<<(std::ostream& os, EncodedArrayValueIterator::ValueType code);

class EncodedStaticFieldValueIterator : public EncodedArrayValueIterator {
 public:
  EncodedStaticFieldValueIterator(const DexFile& dex_file,
                                  const dex::ClassDef& class_def)
      : EncodedArrayValueIterator(dex_file,
                                  dex_file.GetEncodedStaticFieldValuesArray(class_def))
  {}

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EncodedStaticFieldValueIterator);
};

class CallSiteArrayValueIterator : public EncodedArrayValueIterator {
 public:
  CallSiteArrayValueIterator(const DexFile& dex_file,
                             const dex::CallSiteIdItem& call_site_id)
      : EncodedArrayValueIterator(dex_file,
                                  dex_file.GetCallSiteEncodedValuesArray(call_site_id))
  {}

  uint32_t Size() const { return array_size_; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CallSiteArrayValueIterator);
};

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_H_
