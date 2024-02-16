// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_IMPL_MACROS_H_
#define UI_BASE_METADATA_METADATA_IMPL_MACROS_H_

#include <memory>
#include <utility>

#include "ui/base/metadata/metadata_cache.h"
#include "ui/base/metadata/metadata_macros_internal.h"
#include "ui/base/metadata/property_metadata.h"

// Generate the implementation of the metadata accessors and internal class with
// additional macros for defining the class' properties.

#define BEGIN_METADATA_BASE(class_name)                     \
  METADATA_REINTERPRET_BASE_CLASS_INTERNAL(                 \
      class_name, METADATA_CLASS_NAME_INTERNAL(class_name)) \
  BEGIN_METADATA_INTERNAL_BASE(                             \
      class_name, METADATA_CLASS_NAME_INTERNAL(class_name), class_name)

#define _BEGIN_NESTED_METADATA(outer_class, class_name)             \
  BEGIN_METADATA_INTERNAL(outer_class::class_name,                  \
                          METADATA_CLASS_NAME_INTERNAL(class_name), \
                          class_name::kAncestorClass)               \
  METADATA_PARENT_CLASS_INTERNAL(class_name::kAncestorClass)

#define _BEGIN_METADATA_SIMPLE(class_name)                          \
  BEGIN_METADATA_INTERNAL(class_name,                               \
                          METADATA_CLASS_NAME_INTERNAL(class_name), \
                          class_name::kAncestorClass)               \
  METADATA_PARENT_CLASS_INTERNAL(class_name::kAncestorClass)

#define _GET_MD_MACRO_NAME(_1, _2, NAME, ...) NAME

// The following macro overloads the above macros. For most cases, only two
// parameters are used. In some instances when a class is nested within another
// class, the first parameter should be the outer scope with the remaining
// parameters same as the non-nested macro.

#define BEGIN_METADATA(class_name, ...)                                 \
  _GET_MD_MACRO_NAME(class_name, ##__VA_ARGS__, _BEGIN_NESTED_METADATA, \
                     _BEGIN_METADATA_SIMPLE)                            \
  (class_name, ##__VA_ARGS__)

// This macro is used for defining template specializations for a templated view
// class. `class_name_alias` is as the name indicates; it's an alias of the
// instantiated template type. This is typically in the form of: `using foo =
// bar<baz>;`. `template_name` is the base name of the templated class such as
// `bar` from the previous alias. END_METADATA works the same as the non-
// templated versions.
#define BEGIN_TEMPLATE_METADATA(class_name_alias, template_name) \
  BEGIN_TEMPLATE_METADATA_INTERNAL(                              \
      class_name_alias, METADATA_CLASS_NAME_INTERNAL(template_name))

#define END_METADATA }

// This will fail to compile if the property accessors aren't in the form of
// SetXXXX and GetXXXX. If an explicit type converter is specified, it must have
// already been specialized. See the comments in type_converter.h for further
// information.
#define ADD_PROPERTY_METADATA(property_type, property_name, ...)         \
  auto property_name##_prop =                                            \
      std::make_unique<METADATA_PROPERTY_TYPE_INTERNAL(                  \
          property_type, property_name, ##__VA_ARGS__)>(#property_name,  \
                                                        #property_type); \
  AddMemberData(std::move(property_name##_prop));

// This will fail to compile if the property accessor isn't in the form of
// GetXXXX. If an explicit type converter is specified, it must have already
// been specialized. See the comments in type_converter.h for further
// information.
#define ADD_READONLY_PROPERTY_METADATA(property_type, property_name, ...) \
  auto property_name##_prop =                                             \
      std::make_unique<METADATA_READONLY_PROPERTY_TYPE_INTERNAL(          \
          property_type, property_name, ##__VA_ARGS__)>(#property_name,   \
                                                        #property_type);  \
  AddMemberData(std::move(property_name##_prop));

// Adds a ui::ClassProperty of key |property_key| to metadata.
// If the property value is an pointer of type |TValue*|, specify
// |property_type| as |TValue| to allow inspecting the actually value.
// Otherwise the metadata will treat the pointer as it is.
#define ADD_CLASS_PROPERTY_METADATA(property_type, property_key, ...)   \
  auto property_key##_prop =                                            \
      std::make_unique<METADATA_CLASS_PROPERTY_TYPE_INTERNAL(           \
          property_type, property_key, ##__VA_ARGS__)>(property_key,    \
                                                       #property_type); \
  AddMemberData(std::move(property_key##_prop));

#endif  // UI_BASE_METADATA_METADATA_IMPL_MACROS_H_
