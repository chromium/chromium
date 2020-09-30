// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_
#define UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_

#include <memory>
#include <string>
#include <utility>

#include "ui/views/metadata/metadata_cache.h"
#include "ui/views/metadata/metadata_macros_internal.h"
#include "ui/views/metadata/property_metadata.h"

// Generate the implementation of the metadata accessors and internal class with
// additional macros for defining the class' properties.

#define BEGIN_METADATA_BASE(class_name) BEGIN_METADATA_INTERNAL(class_name)

#define BEGIN_METADATA(class_name, parent_class_name)                  \
  static_assert(std::is_base_of<parent_class_name, class_name>::value, \
                "class not child of parent");                          \
  BEGIN_METADATA_INTERNAL(class_name)                                  \
  METADATA_PARENT_CLASS_INTERNAL(parent_class_name)

#define END_METADATA }

// This will fail to compile if the property accessors aren't in the form of
// SetXXXX and GetXXXX.
#define ADD_PROPERTY_METADATA(property_type, property_name)                    \
  std::unique_ptr<METADATA_PROPERTY_TYPE_INTERNAL(property_type,               \
                                                  property_name)>              \
      property_name##_prop = std::make_unique<METADATA_PROPERTY_TYPE_INTERNAL( \
          property_type, property_name)>(#property_name, #property_type);      \
  AddMemberData(std::move(property_name##_prop));

// This will fail to compile if the property accessor isn't in the form of
// GetXXXX.
#define ADD_READONLY_PROPERTY_METADATA(property_type, property_name)          \
  std::unique_ptr<METADATA_READONLY_PROPERTY_TYPE_INTERNAL(property_type,     \
                                                           property_name)>    \
      property_name##_prop =                                                  \
          std::make_unique<METADATA_READONLY_PROPERTY_TYPE_INTERNAL(          \
              property_type, property_name)>(#property_name, #property_type); \
  AddMemberData(std::move(property_name##_prop));

#endif  // UI_VIEWS_METADATA_METADATA_IMPL_MACROS_H_
