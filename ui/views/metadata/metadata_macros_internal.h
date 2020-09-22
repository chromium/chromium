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
#define METADATA_FUNCTION_PREFIX_INTERNAL(class_name) \
  class_name::METADATA_CLASS_NAME_INTERNAL(class_name)

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

#define METADATA_PROPERTY_TYPE_INTERNAL(property_type, property_name)     \
  views::metadata::ClassPropertyMetaData<                                 \
      ViewClass, property_type, decltype(&ViewClass::Set##property_name), \
      &ViewClass::Set##property_name,                                     \
      decltype(std::declval<ViewClass>().Get##property_name()),           \
      &ViewClass::Get##property_name>

#define METADATA_READONLY_PROPERTY_TYPE_INTERNAL(property_type, property_name) \
  views::metadata::ClassPropertyReadOnlyMetaData<                              \
      ViewClass, property_type,                                                \
      decltype(std::declval<ViewClass>().Get##property_name()),                \
      &ViewClass::Get##property_name>

#define BEGIN_METADATA_INTERNAL(class_name)                                 \
  views::metadata::ClassMetaData* class_name::METADATA_CLASS_NAME_INTERNAL( \
      class_name)::meta_data_ = nullptr;                                    \
                                                                            \
  views::metadata::ClassMetaData* class_name::MetaData() {                  \
    if (!METADATA_CLASS_NAME_INTERNAL(class_name)::meta_data_) {            \
      METADATA_CLASS_NAME_INTERNAL(class_name)::meta_data_ =                \
          views::metadata::MakeAndRegisterClassInfo<                        \
              METADATA_CLASS_NAME_INTERNAL(class_name)>();                  \
    }                                                                       \
    return METADATA_CLASS_NAME_INTERNAL(class_name)::meta_data_;            \
  }                                                                         \
                                                                            \
  views::metadata::ClassMetaData* class_name::GetClassMetaData() {          \
    return MetaData();                                                      \
  }                                                                         \
                                                                            \
  const char* class_name::GetClassName() const {                            \
    return class_name::kViewClassName;                                      \
  }                                                                         \
  const char class_name::kViewClassName[] = #class_name;                    \
                                                                            \
  void METADATA_FUNCTION_PREFIX_INTERNAL(class_name)::BuildMetaData() {     \
    SetTypeName(std::string(#class_name));

#define METADATA_PARENT_CLASS_INTERNAL(parent_class_name) \
  SetParentClassMetaData(parent_class_name::MetaData());

#endif  // UI_VIEWS_METADATA_METADATA_MACROS_INTERNAL_H_
