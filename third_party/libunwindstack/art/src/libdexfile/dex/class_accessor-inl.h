/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_
#define ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_

#include "class_accessor.h"

#include "base/hiddenapi_flags.h"
#include "base/leb128.h"
#include "base/utils.h"
#include "class_iterator.h"
#include "code_item_accessors-inl.h"
#include "dex_file.h"
#include "method_reference.h"

namespace art {

inline ClassAccessor::ClassAccessor(const ClassIteratorData& data)
    : ClassAccessor(data.dex_file_, data.class_def_idx_) {}

inline ClassAccessor::ClassAccessor(const DexFile& dex_file,
                                    const dex::ClassDef& class_def,
                                    bool parse_hiddenapi_class_data)
    : ClassAccessor(dex_file,
                    dex_file.GetClassData(class_def),
                    dex_file.GetIndexForClassDef(class_def),
                    parse_hiddenapi_class_data) {}

inline ClassAccessor::ClassAccessor(const DexFile& dex_file, uint32_t class_def_index)
    : ClassAccessor(dex_file, dex_file.GetClassDef(class_def_index)) {}

inline ClassAccessor::ClassAccessor(const DexFile& dex_file,
                                    const uint8_t* class_data,
                                    uint32_t class_def_index,
                                    bool parse_hiddenapi_class_data)
    : dex_file_(dex_file),
      class_def_index_(class_def_index),
      ptr_pos_(class_data),
      hiddenapi_ptr_pos_(nullptr),
      num_static_fields_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_instance_fields_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_direct_methods_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u),
      num_virtual_methods_(ptr_pos_ != nullptr ? DecodeUnsignedLeb128(&ptr_pos_) : 0u) {
  if (parse_hiddenapi_class_data && class_def_index != DexFile::kDexNoIndex32) {
    const dex::HiddenapiClassData* hiddenapi_class_data = dex_file.GetHiddenapiClassData();
    if (hiddenapi_class_data != nullptr) {
      hiddenapi_ptr_pos_ = hiddenapi_class_data->GetFlagsPointer(class_def_index);
    }
  }
}

inline void ClassAccessor::Method::Read() {
  index_ += DecodeUnsignedLeb128(&ptr_pos_);
  access_flags_ = DecodeUnsignedLeb128(&ptr_pos_);
  code_off_ = DecodeUnsignedLeb128(&ptr_pos_);
  if (hiddenapi_ptr_pos_ != nullptr) {
    hiddenapi_flags_ = DecodeUnsignedLeb128(&hiddenapi_ptr_pos_);
    DCHECK(hiddenapi::ApiList(hiddenapi_flags_).IsValid());
  }
}

inline MethodReference ClassAccessor::Method::GetReference() const {
  return MethodReference(&dex_file_, GetIndex());
}


inline void ClassAccessor::Field::Read() {
  index_ += DecodeUnsignedLeb128(&ptr_pos_);
  access_flags_ = DecodeUnsignedLeb128(&ptr_pos_);
  if (hiddenapi_ptr_pos_ != nullptr) {
    hiddenapi_flags_ = DecodeUnsignedLeb128(&hiddenapi_ptr_pos_);
    DCHECK(hiddenapi::ApiList(hiddenapi_flags_).IsValid());
  }
}

template <typename DataType, typename Visitor>
inline void ClassAccessor::VisitMembers(size_t count,
                                        const Visitor& visitor,
                                        DataType* data) const {
  DCHECK(data != nullptr);
  for ( ; count != 0; --count) {
    data->Read();
    visitor(*data);
  }
}

template <typename StaticFieldVisitor,
          typename InstanceFieldVisitor,
          typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitFieldsAndMethods(
    const StaticFieldVisitor& static_field_visitor,
    const InstanceFieldVisitor& instance_field_visitor,
    const DirectMethodVisitor& direct_method_visitor,
    const VirtualMethodVisitor& virtual_method_visitor) const {
  Field field(dex_file_, ptr_pos_, hiddenapi_ptr_pos_);
  VisitMembers(num_static_fields_, static_field_visitor, &field);
  field.NextSection();
  VisitMembers(num_instance_fields_, instance_field_visitor, &field);

  Method method(dex_file_, field.ptr_pos_, field.hiddenapi_ptr_pos_, /*is_static_or_direct*/ true);
  VisitMembers(num_direct_methods_, direct_method_visitor, &method);
  method.NextSection();
  VisitMembers(num_virtual_methods_, virtual_method_visitor, &method);
}

template <typename DirectMethodVisitor,
          typename VirtualMethodVisitor>
inline void ClassAccessor::VisitMethods(const DirectMethodVisitor& direct_method_visitor,
                                        const VirtualMethodVisitor& virtual_method_visitor) const {
  VisitFieldsAndMethods(VoidFunctor(),
                        VoidFunctor(),
                        direct_method_visitor,
                        virtual_method_visitor);
}

template <typename StaticFieldVisitor,
          typename InstanceFieldVisitor>
inline void ClassAccessor::VisitFields(const StaticFieldVisitor& static_field_visitor,
                                       const InstanceFieldVisitor& instance_field_visitor) const {
  VisitFieldsAndMethods(static_field_visitor,
                        instance_field_visitor,
                        VoidFunctor(),
                        VoidFunctor());
}

inline const dex::CodeItem* ClassAccessor::GetCodeItem(const Method& method) const {
  return dex_file_.GetCodeItem(method.GetCodeItemOffset());
}

inline CodeItemInstructionAccessor ClassAccessor::Method::GetInstructions() const {
  return CodeItemInstructionAccessor(dex_file_, dex_file_.GetCodeItem(GetCodeItemOffset()));
}

inline CodeItemDataAccessor ClassAccessor::Method::GetInstructionsAndData() const {
  return CodeItemDataAccessor(dex_file_, dex_file_.GetCodeItem(GetCodeItemOffset()));
}

inline const char* ClassAccessor::GetDescriptor() const {
  return dex_file_.StringByTypeIdx(GetClassIdx());
}

inline const dex::CodeItem* ClassAccessor::Method::GetCodeItem() const {
  return dex_file_.GetCodeItem(code_off_);
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>>
    ClassAccessor::GetFieldsInternal(size_t count) const {
  return {
      DataIterator<Field>(dex_file_,
                          0u,
                          num_static_fields_,
                          count,
                          ptr_pos_,
                          hiddenapi_ptr_pos_),
      DataIterator<Field>(dex_file_,
                          count,
                          num_static_fields_,
                          count,
                          // The following pointers are bogus but unused in the `end` iterator.
                          ptr_pos_,
                          hiddenapi_ptr_pos_) };
}

// Return an iteration range for the first <count> methods.
inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Method>>
    ClassAccessor::GetMethodsInternal(size_t count) const {
  // Skip over the fields.
  Field field(dex_file_, ptr_pos_, hiddenapi_ptr_pos_);
  VisitMembers(NumFields(), VoidFunctor(), &field);
  // Return the iterator pair.
  return {
      DataIterator<Method>(dex_file_,
                           0u,
                           num_direct_methods_,
                           count,
                           field.ptr_pos_,
                           field.hiddenapi_ptr_pos_),
      DataIterator<Method>(dex_file_,
                           count,
                           num_direct_methods_,
                           count,
                           // The following pointers are bogus but unused in the `end` iterator.
                           field.ptr_pos_,
                           field.hiddenapi_ptr_pos_) };
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>> ClassAccessor::GetFields()
    const {
  return GetFieldsInternal(num_static_fields_ + num_instance_fields_);
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>>
    ClassAccessor::GetStaticFields() const {
  return GetFieldsInternal(num_static_fields_);
}


inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>>
    ClassAccessor::GetInstanceFields() const {
  IterationRange<ClassAccessor::DataIterator<ClassAccessor::Field>> fields = GetFields();
  // Skip the static fields.
  return { std::next(fields.begin(), NumStaticFields()), fields.end() };
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Method>>
    ClassAccessor::GetMethods() const {
  return GetMethodsInternal(NumMethods());
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Method>>
    ClassAccessor::GetDirectMethods() const {
  return GetMethodsInternal(NumDirectMethods());
}

inline IterationRange<ClassAccessor::DataIterator<ClassAccessor::Method>>
    ClassAccessor::GetVirtualMethods() const {
  IterationRange<DataIterator<Method>> methods = GetMethods();
  // Skip the direct fields.
  return { std::next(methods.begin(), NumDirectMethods()), methods.end() };
}

inline dex::TypeIndex ClassAccessor::GetClassIdx() const {
  return dex_file_.GetClassDef(class_def_index_).class_idx_;
}

inline const dex::ClassDef& ClassAccessor::GetClassDef() const {
  return dex_file_.GetClassDef(GetClassDefIndex());
}

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_CLASS_ACCESSOR_INL_H_
