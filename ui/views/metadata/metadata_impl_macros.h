// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_
#define UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_

#include "ui/views/metadata/metadata_cache.h"
#include "ui/views/metadata/metadata_macros_internal.h"
#include "ui/views/metadata/property_metadata.h"

// Generate the implementation of the metadata accessors and internal class with
// additional macros for defining the class' properties.

#define BEGIN_METADATA(class_name)                                          \
  views::metadata::ClassMetaData* class_name::METADATA_CLASS_NAME_INTERNAL( \
      class_name)::meta_data_ = nullptr;                                    \
                                                                            \
  views::metadata::ClassMetaData* class_name::MetaData() {                  \
    if (!METADATA_CLASS_NAME_INTERNAL(class_name)::meta_data_)              \
      METADATA_CLASS_NAME_INTERNAL(class_name)::meta_data_ =                \
          views::metadata::MakeAndRegisterClassInfo<                        \
              METADATA_CLASS_NAME_INTERNAL(class_name)>();                  \
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

#define END_METADATA() }

#define METADATA_PARENT_CLASS(parent_class_name) \
  SetParentClassMetaData(parent_class_name::MetaData());

// This will fail to compile if the property accessors aren't in the form of
// SetXXXX and GetXXXX.
#define ADD_PROPERTY_METADATA(class_name, property_type, property_name)        \
  std::unique_ptr<METADATA_PROPERTY_TYPE_INTERNAL(class_name, property_type,   \
                                                  property_name)>              \
      property_name##_prop = std::make_unique<METADATA_PROPERTY_TYPE_INTERNAL( \
          class_name, property_type, property_name)>();                        \
  property_name##_prop->SetMemberName(#property_name);                         \
  property_name##_prop->SetMemberType(#property_type);                         \
  AddMemberData(std::move(property_name##_prop));

// This will fail to compile if the property accessor isn't in the form of
// GetXXXX.
#define ADD_READONLY_PROPERTY_METADATA(class_name, property_type,    \
                                       property_name)                \
  std::unique_ptr<METADATA_READONLY_PROPERTY_TYPE_INTERNAL(          \
      class_name, property_type, property_name)>                     \
      property_name##_prop =                                         \
          std::make_unique<METADATA_READONLY_PROPERTY_TYPE_INTERNAL( \
              class_name, property_type, property_name)>();          \
  property_name##_prop->SetMemberName(#property_name);               \
  property_name##_prop->SetMemberType(#property_type);               \
  AddMemberData(std::move(property_name##_prop));

#endif  // UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_
