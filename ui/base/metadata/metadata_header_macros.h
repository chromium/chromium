// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
#define UI_BASE_METADATA_METADATA_HEADER_MACROS_H_

#include "ui/base/metadata/metadata_macros_internal.h"

// Generate Metadata's accessor functions and internal class declaration.
// This should be used in a header file of the View class or its subclasses.
#define METADATA_HEADER(class_name)       \
  METADATA_ACCESSORS_INTERNAL(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

// A version of METADATA_HEADER for View, the root of the metadata hierarchy.
// Here METADATA_ACCESSORS_INTERNAL_BASE is called.
#define METADATA_HEADER_BASE(class_name)       \
  METADATA_ACCESSORS_INTERNAL_BASE(class_name) \
  METADATA_CLASS_INTERNAL(class_name, __FILE__, __LINE__)

#endif  // UI_BASE_METADATA_METADATA_HEADER_MACROS_H_
