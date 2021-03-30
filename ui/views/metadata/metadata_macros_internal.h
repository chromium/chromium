// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_
#define UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "ui/views/metadata/metadata_types.h"

// Internal Metadata Generation Helpers ---------------------------------------

#define METADATA_CLASS_NAME_INTERNAL(class_name) class_name##_MetaData

// Metadata Accessors ---------------------------------------------------------
#define METADATA_ACCESSORS_INTERNAL(class_name)      \
  static const char kViewClassName[];                \
  const char* GetClassName() const override;         \
  static views::metadata::ClassMetaData* MetaData(); \
  views::metadata::ClassMetaData* GetClassMetaData() override;

// A version of METADATA_ACCESSORS_INTERNAL for View, the root of the metadata
// hierarchy; here GetClassName() is not declared as an override.
#define METADATA_ACCESSORS_INTERNAL_BASE(class_name) \
  static const char kViewClassName[];                \
  virtual const char* GetClassName() const;          \
  static views::metadata::ClassMetaData* MetaData(); \
  views::metadata::ClassMetaData* GetClassMetaData() override;

// Metadata Class -------------------------------------------------------------
#define METADATA_CLASS_INTERNAL(class_name, file, line)                  \
  class METADATA_CLASS_NAME_INTERNAL(class_name)                         \
      : public views::metadata::ClassMetaData {                          \
   public:                                                               \
    using ViewClass = class_name;                                        \
    explicit METADATA_CLASS_NAME_INTERNAL(class_name)()                  \
        : ClassMetaData(file, line) {                                    \
      BuildMetaData();                                                   \
    }                                                                    \
    METADATA_CLASS_NAME_INTERNAL(class_name)                             \
    (const METADATA_CLASS_NAME_INTERNAL(class_name) &) = delete;         \
    METADATA_CLASS_NAME_INTERNAL(class_name) & operator=(                \
        const METADATA_CLASS_NAME_INTERNAL(class_name) &) = delete;      \
                                                                         \
   private:                                                              \
    friend class class_name;                                             \
    virtual void BuildMetaData();                                        \
    static views::metadata::ClassMetaData* meta_data_ ALLOW_UNUSED_TYPE; \
  }

#define METADATA_PROPERTY_TYPE_INTERNAL(property_type, property_name, ...) \
  views::metadata::ObjectPropertyMetaData<                                 \
      ViewClass, property_type, decltype(&ViewClass::Set##property_name),  \
      &ViewClass::Set##property_name,                                      \
      decltype(std::declval<ViewClass>().Get##property_name()),            \
      &ViewClass::Get##property_name, ##__VA_ARGS__>

#define METADATA_READONLY_PROPERTY_TYPE_INTERNAL(property_type, property_name, \
                                                 ...)                          \
  views::metadata::ObjectPropertyReadOnlyMetaData<                             \
      ViewClass, property_type,                                                \
      decltype(std::declval<ViewClass>().Get##property_name()),                \
      &ViewClass::Get##property_name, ##__VA_ARGS__>

#define METADATA_CLASS_PROPERTY_TYPE_INTERNAL(property_type, property_key, \
                                              ...)                         \
  views::metadata::ClassPropertyMetaData<decltype(property_key),           \
                                         property_type, ##__VA_ARGS__>

#define BEGIN_METADATA_INTERNAL(qualified_class_name, metadata_class_name,   \
                                parent_class_name)                           \
  views::metadata::ClassMetaData*                                            \
      qualified_class_name::metadata_class_name::meta_data_ = nullptr;       \
                                                                             \
  views::metadata::ClassMetaData* qualified_class_name::MetaData() {         \
    static_assert(                                                           \
        std::is_base_of<parent_class_name, qualified_class_name>::value,     \
        "class not child of parent");                                        \
    if (!qualified_class_name::metadata_class_name::meta_data_) {            \
      qualified_class_name::metadata_class_name::meta_data_ =                \
          views::metadata::MakeAndRegisterClassInfo<                         \
              qualified_class_name::metadata_class_name>();                  \
    }                                                                        \
    return qualified_class_name::metadata_class_name::meta_data_;            \
  }                                                                          \
                                                                             \
  views::metadata::ClassMetaData* qualified_class_name::GetClassMetaData() { \
    return MetaData();                                                       \
  }                                                                          \
                                                                             \
  const char* qualified_class_name::GetClassName() const {                   \
    return kViewClassName;                                                   \
  }                                                                          \
  const char qualified_class_name::kViewClassName[] = #qualified_class_name; \
                                                                             \
  void qualified_class_name::metadata_class_name::BuildMetaData() {          \
    SetTypeName(std::string(#qualified_class_name));

#define METADATA_PARENT_CLASS_INTERNAL(parent_class_name) \
  SetParentClassMetaData(parent_class_name::MetaData());

#endif  // UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_
