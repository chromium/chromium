// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_TYPES_H_
#define UI_BASE_METADATA_METADATA_TYPES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ui {
namespace metadata {

enum class PropertyFlags : uint32_t {
  // By default, properties are read/write. This flag indicates that the given
  // property metadata instance needs no special attention.
  kEmpty = 0x00,
  // Property metadata instance should be treated as read-only. SetValueAsString
  // should not be called since there may not be a conversion from a string for
  // the type of the property. (see kIsSerializable below for additional info).
  // Calling SetValueAsString() may trigger a NOTREACHED() error under debug.
  kReadOnly = 0x01,
  // Property metadata can be serialized to or from a string. Needs to make sure
  // this flag is set to have meaningful SetValueAsString() and
  // GetValueFromString(). This is ultimately a signal indicating the underlying
  // TypeConverter is able to convert the value to/from a string.
  kSerializable = 0x100,
};

COMPONENT_EXPORT(UI_BASE_METADATA)
extern PropertyFlags operator|(PropertyFlags op1, PropertyFlags op2);
COMPONENT_EXPORT(UI_BASE_METADATA)
extern PropertyFlags operator&(PropertyFlags op1, PropertyFlags op2);
COMPONENT_EXPORT(UI_BASE_METADATA)
extern PropertyFlags operator^(PropertyFlags op1, PropertyFlags op2);
COMPONENT_EXPORT(UI_BASE_METADATA) extern bool operator!(PropertyFlags op);

// Used to identify the CallbackList<> within the PropertyChangedVectors map.
using PropertyKey = const void*;

// Used to generate property keys when a single field needs multiple property
// keys - for example, if you have a single "bounds" field which is a gfx::Rect
// and want to have four separate properties that all use that field, you can
// use MakeUniquePropertyKey() rather than using &bounds_ + 0, &bounds_ + 1,
// and so on. This avoids unsafe buffer use warnings.
static inline PropertyKey MakeUniquePropertyKey(PropertyKey base,
                                                uintptr_t offset) {
  return reinterpret_cast<PropertyKey>(reinterpret_cast<uintptr_t>(base) +
                                       offset);
}

using PropertyChangedCallbacks = base::RepeatingClosureList;
using PropertyChangedCallback = PropertyChangedCallbacks::CallbackType;

// Interface for classes that provide ClassMetaData (via macros in
// metadata_header_macros.h). GetClassMetaData() is automatically overridden and
// implemented in the relevant macros, so a class must merely have
// MetaDataProvider somewhere in its ancestry.
class COMPONENT_EXPORT(UI_BASE_METADATA) MetaDataProvider {
 public:
  MetaDataProvider();
  virtual ~MetaDataProvider();
  virtual const class ClassMetaData* GetClassMetaData() const = 0;
  class ClassMetaData* GetClassMetaData();

 protected:
  [[nodiscard]] base::CallbackListSubscription AddPropertyChangedCallback(
      PropertyKey property,
      PropertyChangedCallback callback);
  void TriggerChangedCallback(PropertyKey property);

 private:
  using PropertyChangedVectors =
      std::map<PropertyKey, std::unique_ptr<PropertyChangedCallbacks>>;

  // Property Changed Callbacks ------------------------------------------------
  PropertyChangedVectors property_changed_vectors_;
};

class MemberMetaDataBase;

// Represents the 'meta data' that describes a class. Using the appropriate
// macros in ui/base/metadata/metadata_impl_macros.h, a descendant of this
// class is declared within the scope of the containing class. See information
// about using the macros in the comment for the views::View class.
class COMPONENT_EXPORT(UI_BASE_METADATA) ClassMetaData {
 public:
  ClassMetaData();
  ClassMetaData(std::string file, int line);
  ClassMetaData(const ClassMetaData&) = delete;
  ClassMetaData& operator=(const ClassMetaData&) = delete;
  virtual ~ClassMetaData();

  const char* type_name() const {
    static_assert(
        std::is_same<decltype(type_name_), std::string_view>::value,
        "This string is logged in plaintext via UMA trace events uploads, so "
        "must be static as a privacy requirement.");
    // This is safe because the underlying string is a C string and null
    // terminated.
    // TODO(325589481): See if directly returning the string_view would be
    // desirable.
    return type_name_.data();
  }
  const std::vector<raw_ptr<MemberMetaDataBase, VectorExperimental>>& members()
      const {
    return members_;
  }
  const std::string& file() const { return file_; }
  const int& line() const { return line_; }
  const std::string& GetUniqueName() const;
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
  class COMPONENT_EXPORT(UI_BASE_METADATA) ClassMemberIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = MemberMetaDataBase*;
    using difference_type = std::ptrdiff_t;
    using pointer = MemberMetaDataBase**;
    using reference = MemberMetaDataBase*&;

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

    raw_ptr<ClassMetaData> current_collection_;
    size_t current_vector_index_;
  };

  ClassMemberIterator begin();
  ClassMemberIterator end();

 protected:
  void SetTypeName(std::string_view type_name);

 private:
  // `type_name_` is a static string stored in the binary.
  std::string_view type_name_;
  mutable std::string unique_name_;
  std::vector<raw_ptr<MemberMetaDataBase, VectorExperimental>> members_;
  raw_ptr<ClassMetaData> parent_class_meta_data_ = nullptr;
  std::string file_;
  const int line_ = 0;
};

// Abstract base class to represent meta data about class members.
// Provides basic information (such as the name of the member), and templated
// accessors to get/set the value of the member on an object.
class COMPONENT_EXPORT(UI_BASE_METADATA) MemberMetaDataBase {
 public:
  using ValueStrings = std::vector<std::u16string>;
  MemberMetaDataBase(const std::string& member_name,
                     const std::string& member_type)
      : member_name_(member_name), member_type_(member_type) {}
  MemberMetaDataBase(const MemberMetaDataBase&) = delete;
  MemberMetaDataBase& operator=(const MemberMetaDataBase&) = delete;
  virtual ~MemberMetaDataBase() = default;

  // Access the value of this member and return it as a string.
  // |obj| is the instance on which to obtain the value of the property this
  // metadata represents.
  virtual std::u16string GetValueAsString(void* obj) const = 0;

  // Set the value of this member through a string on a specified object.
  // |obj| is the instance on which to set the value of the property this
  // metadata represents.
  virtual void SetValueAsString(void* obj, const std::u16string& new_value);

  // Return various information flags about the property.
  virtual PropertyFlags GetPropertyFlags() const = 0;

  // Return a list of valid property values as a vector of strings. An empty
  // vector indicates that the natural limits of the underlying type applies.
  virtual ValueStrings GetValidValues() const;

  // Return an optional prefix string used by the ui-devtools frontend to
  // prepend to the member name which causes a special value editor to become
  // available. For instance, an SkColor member type would add the "--" string
  // which tells the frontend to display a color swatch and a color editing
  // dialog.
  virtual const char* GetMemberNamePrefix() const;

  const std::string& member_name() const { return member_name_; }
  const std::string& member_type() const { return member_type_; }

 private:
  std::string member_name_;
  std::string member_type_;
};  // class MemberMetaDataBase

}  // namespace metadata
}  // namespace ui

#endif  // UI_BASE_METADATA_METADATA_TYPES_H_
