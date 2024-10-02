// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/metadata/metadata_types.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "ui/base/metadata/base_type_conversion.h"

namespace ui {
namespace metadata {

PropertyFlags operator|(PropertyFlags op1, PropertyFlags op2) {
  return static_cast<PropertyFlags>(static_cast<uint32_t>(op1) |
                                    static_cast<uint32_t>(op2));
}

PropertyFlags operator&(PropertyFlags op1, PropertyFlags op2) {
  return static_cast<PropertyFlags>(static_cast<uint32_t>(op1) &
                                    static_cast<uint32_t>(op2));
}

PropertyFlags operator^(PropertyFlags op1, PropertyFlags op2) {
  return static_cast<PropertyFlags>(static_cast<uint32_t>(op1) ^
                                    static_cast<uint32_t>(op2));
}

bool operator!(PropertyFlags op) {
  return !static_cast<bool>(op);
}

MetaDataProvider::MetaDataProvider() = default;

MetaDataProvider::~MetaDataProvider() = default;

class ClassMetaData* MetaDataProvider::GetClassMetaData() {
  return const_cast<ClassMetaData*>(std::as_const(*this).GetClassMetaData());
}

base::CallbackListSubscription MetaDataProvider::AddPropertyChangedCallback(
    PropertyKey property,
    PropertyChangedCallback callback) {
  auto entry = property_changed_vectors_.find(property);
  if (entry == property_changed_vectors_.end()) {
    entry = property_changed_vectors_
                .emplace(property, std::make_unique<PropertyChangedCallbacks>())
                .first;
  }
  PropertyChangedCallbacks* property_changed_callbacks = entry->second.get();

  return property_changed_callbacks->Add(std::move(callback));
}

void MetaDataProvider::TriggerChangedCallback(PropertyKey property) {
  auto entry = property_changed_vectors_.find(property);
  if (entry == property_changed_vectors_.end()) {
    return;
  }

  PropertyChangedCallbacks* property_changed_callbacks = entry->second.get();
  property_changed_callbacks->Notify();
}

ClassMetaData::ClassMetaData() = default;

ClassMetaData::ClassMetaData(std::string file, int line) : line_(line) {
  base::TrimString(file, "./\\", &file_);
}

ClassMetaData::~ClassMetaData() = default;

const std::string& ClassMetaData::GetUniqueName() const {
  if (unique_name_.empty()) {
    unique_name_ = base::StrCat({file_, ":", type_name_});
  }
  return unique_name_;
}

void ClassMetaData::AddMemberData(
    std::unique_ptr<MemberMetaDataBase> member_data) {
  members_.push_back(member_data.release());
}

MemberMetaDataBase* ClassMetaData::FindMemberData(
    const std::string& member_name) {
  for (MemberMetaDataBase* member_data : members_) {
    if (member_data->member_name() == member_name) {
      return member_data;
    }
  }

  if (parent_class_meta_data_ != nullptr)
    return parent_class_meta_data_->FindMemberData(member_name);

  return nullptr;
}

/** Member Iterator */
ClassMetaData::ClassMemberIterator::ClassMemberIterator(
    const ClassMetaData::ClassMemberIterator& other) {
  current_collection_ = other.current_collection_;
  current_vector_index_ = other.current_vector_index_;
}
ClassMetaData::ClassMemberIterator::~ClassMemberIterator() = default;

// If starting_container's members vector is empty, set current_collection_
// to its parent until parent class has members. Base parent class View
// will always have members, even if all other parent classes do not.
ClassMetaData::ClassMemberIterator::ClassMemberIterator(
    ClassMetaData* starting_container) {
  current_collection_ = starting_container;
  if (!current_collection_) {
    current_vector_index_ = SIZE_MAX;
  } else if (current_collection_->members().size() == 0) {
    do {
      current_collection_ = current_collection_->parent_class_meta_data();
    } while (current_collection_ && current_collection_->members().empty());
    current_vector_index_ = (current_collection_ ? 0 : SIZE_MAX);
  } else {
    current_vector_index_ = 0;
  }
}

bool ClassMetaData::ClassMemberIterator::operator==(
    const ClassMemberIterator& rhs) const {
  return current_vector_index_ == rhs.current_vector_index_ &&
         current_collection_ == rhs.current_collection_;
}

ClassMetaData::ClassMemberIterator&
ClassMetaData::ClassMemberIterator::operator++() {
  IncrementHelper();
  return *this;
}

ClassMetaData::ClassMemberIterator
ClassMetaData::ClassMemberIterator::operator++(int) {
  ClassMetaData::ClassMemberIterator tmp(*this);
  IncrementHelper();
  return tmp;
}

bool ClassMetaData::ClassMemberIterator::IsLastMember() const {
  return current_vector_index_ == current_collection_->members().size() - 1;
}

std::string ClassMetaData::ClassMemberIterator::GetCurrentCollectionName()
    const {
  return std::string(current_collection_->type_name());
}

void ClassMetaData::ClassMemberIterator::IncrementHelper() {
  DCHECK_LT(current_vector_index_, SIZE_MAX);
  ++current_vector_index_;

  if (current_vector_index_ >= current_collection_->members().size()) {
    do {
      current_collection_ = current_collection_->parent_class_meta_data();
      current_vector_index_ = (current_collection_ ? 0 : SIZE_MAX);
    } while (current_collection_ && current_collection_->members().empty());
  }
}

ClassMetaData::ClassMemberIterator ClassMetaData::begin() {
  return ClassMemberIterator(this);
}

ClassMetaData::ClassMemberIterator ClassMetaData::end() {
  return ClassMemberIterator(nullptr);
}

void ClassMetaData::SetTypeName(std::string_view type_name) {
  type_name_ = type_name;
}

void MemberMetaDataBase::SetValueAsString(void* obj,
                                          const std::u16string& new_value) {
  NOTREACHED();
}

const char* MemberMetaDataBase::GetMemberNamePrefix() const {
  return "";
}

MemberMetaDataBase::ValueStrings MemberMetaDataBase::GetValidValues() const {
  return ValueStrings();
}

}  // namespace metadata
}  // namespace ui
