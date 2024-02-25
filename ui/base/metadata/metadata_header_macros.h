// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
#define UI_BASE_METADATA_METADATA_HEADER_MACROS_H_

#include "ui/base/metadata/metadata_macros_internal.h"
#include "ui/base/metadata/metadata_utils.h"

// Generate Metadata's accessor functions and internal class declaration.
// This should be used in a header file of the View class or its subclasses.
#define _METADATA_HEADER1(class_name)     \
  METADATA_ACCESSORS_INTERNAL(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

#define _METADATA_HEADER2(new_class_name, ancestor_class_name)        \
  static_assert(ui::metadata::kHasClassMetadata<ancestor_class_name>, \
                #ancestor_class_name                                  \
                " doesn't implement metadata. Make "                  \
                "sure class publicly calls METADATA_HEADER in the "   \
                "declaration.");                                      \
                                                                      \
 public:                                                              \
  using kAncestorClass = ancestor_class_name;                         \
  _METADATA_HEADER1(new_class_name);                                  \
                                                                      \
 private:  // NOLINTNEXTLINE

#define METADATA_HEADER(class_name, ancestor_class_name) \
  _METADATA_HEADER2(class_name, ancestor_class_name)

// When adding metadata to a templated class, use this macro. `class_name` is
// the base name of the template class. If the `ancestor_class_name` is also a
// templated class, define a type alias with a `using foo = bar<baz>'`
// statement. Then use that alias for the parameter.
#define METADATA_TEMPLATE_HEADER(class_name, ancestor_class_name)     \
  static_assert(ui::metadata::kHasClassMetadata<ancestor_class_name>, \
                #ancestor_class_name                                  \
                " doesn't implement metadata. Make "                  \
                "sure class publicly calls METADATA_HEADER in the "   \
                "declaration.");                                      \
                                                                      \
 public:                                                              \
  using kAncestorClass = ancestor_class_name;                         \
  METADATA_ACCESSORS_INTERNAL_TEMPLATE(class_name)                    \
  METADATA_CLASS_INTERNAL_TEMPLATE(class_name, __FILE__, __LINE__);   \
                                                                      \
 private:

// A version of METADATA_HEADER for View, the root of the metadata hierarchy.
// Here METADATA_ACCESSORS_INTERNAL_BASE is called.
#define METADATA_HEADER_BASE(class_name)       \
  METADATA_ACCESSORS_INTERNAL_BASE(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

#define DECLARE_TEMPLATE_METADATA(class_name_alias, template_name) \
  DECLARE_TEMPLATE_METADATA_INTERNAL(                              \
      class_name_alias, METADATA_CLASS_NAME_INTERNAL(template_name))

#endif  // UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
