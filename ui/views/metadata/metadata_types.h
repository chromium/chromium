// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_TYPES_H_
#define UI_VIEWS_METADATA_METADATA_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/views_export.h"

namespace views {
namespace metadata {

enum class PropertyFlags {
  // By default, properties are read/write. This flag indicates that the given
  // property metadata instance needs no special attention.
  kEmpty = 0x00,
  // Property metadata instance should be treated as read-only. Calling
  // SetValueAsString() will trigger a NOTREACHED() error under debug.
  kReadOnly = 0x01,
};

// Interface for classes that provide ClassMetaData (via macros in
// metadata_header_macros.h). GetClassMetaData() is automatically overridden and
// implemented in the relevant macros, so a class must merely have
// MetaDataProvider somewhere in its ancestry.
class MetaDataProvider {
 public:
  virtual class ClassMetaData* GetClassMetaData() = 0;
};

class MemberMetaDataBase;

// Represents the 'meta data' that describes a class. Using the appropriate
// macros in ui/views/metadata/metadata_impl_macros.h, a descendant of this
// class is declared within the scope of the containing class. See information
// about using the macros in the comment for the views::View class.
class VIEWS_EXPORT ClassMetaData {
 public:
  ClassMetaData();
  ClassMetaData(std::string file, int line);
  virtual ~ClassMetaData();

  const std::string& type_name() const { return type_name_; }
  const std::vector<MemberMetaDataBase*>& members() const { return members_; }
  const std::string& file() const { return file_; }
  const int& line() const { return line_; }
  void AddMemberData(std::unique_ptr<MemberMetaDataBase> member_data);

  // Lookup the member data entry for a member of this class with a given name.
  // Returns the appropriate MemberMetaDataBase* if it exists, nullptr
  // otherwise.
  MemberMetaDataBase* FindMemberData(const std::string& member_name);

  ClassMetaData* parent_class_meta_data() const {
    return parent_class_meta_data_;
  }
  void SetParentClassMetaData(ClassMetaData* parent_meta_data) {
    parent_class_meta_data_ = parent_meta_data;
  }

  // Custom iterator to iterate through all member data entries associated with
  // a class (including members declared in parent classes).
  // Example:
  //    for(views::MemberMetaDataBase* member : class_meta_data) {
  //      OperateOn(member);
  //    }
  class VIEWS_EXPORT ClassMemberIterator
      : public std::iterator<std::forward_iterator_tag, MemberMetaDataBase*> {
   public:
    ClassMemberIterator(const ClassMemberIterator& other);
    ~ClassMemberIterator();

    ClassMemberIterator& operator++();
    ClassMemberIterator operator++(int);

    bool operator==(const ClassMemberIterator& rhs) const;
    bool operator!=(const ClassMemberIterator& rhs) const {
      return !(*this == rhs);
    }

    MemberMetaDataBase* operator*() {
      if (current_collection_ == nullptr ||
          current_vector_index_ >= current_collection_->members().size())
        return nullptr;

      return current_collection_->members()[current_vector_index_];
    }

    // Returns true if iterator currently on last member for that current
    // collection.
    bool IsLastMember() const;

    std::string GetCurrentCollectionName() const;

   private:
    friend class ClassMetaData;
    explicit ClassMemberIterator(ClassMetaData* starting_container);
    void IncrementHelper();

    ClassMetaData* current_collection_;
    size_t current_vector_index_;
  };

  ClassMemberIterator begin();
  ClassMemberIterator end();

 protected:
  void SetTypeName(const std::string& type_name);

 private:
  std::string type_name_;
  std::vector<MemberMetaDataBase*> members_;
  ClassMetaData* parent_class_meta_data_ = nullptr;
  std::string file_;
  const int line_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClassMetaData);
};

// Abstract base class to represent meta data about class members.
// Provides basic information (such as the name of the member), and templated
// accessors to get/set the value of the member on an object.
class VIEWS_EXPORT MemberMetaDataBase {
 public:
  MemberMetaDataBase() = default;
  virtual ~MemberMetaDataBase() = default;

  // Access the value of this member and return it as a string.
  // |obj| is the instance on which to obtain the value of the property this
  // metadata represents.
  virtual base::string16 GetValueAsString(void* obj) const = 0;

  // Set the value of this member through a string on a specified object.
  // |obj| is the instance on which to set the value of the property this
  // metadata represents.
  virtual void SetValueAsString(void* obj, const base::string16& new_value);

  // Return various information flags about the property.
  virtual PropertyFlags GetPropertyFlags() const = 0;

  void SetMemberName(const char* name) { member_name_ = name; }
  void SetMemberType(const char* type) { member_type_ = type; }
  const std::string& member_name() const { return member_name_; }
  const std::string& member_type() const { return member_type_; }

 private:
  std::string member_name_;
  std::string member_type_;

  DISALLOW_COPY_AND_ASSIGN(MemberMetaDataBase);
};  // class MemberMetaDataBase

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_METADATA_TYPES_H_
