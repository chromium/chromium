// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLASS_PROPERTIES_H_
#define UI_BASE_CLASS_PROPERTIES_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "ui/base/class_property.h"

// Declare common, broadly-used template specializations here to make sure that
// the compiler knows about them before the first template instance use. Using a
// template instance before its specialization is declared in a translation unit
// is an error.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), bool)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), float)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), int)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE),
                                        std::u16string*)

#endif  // UI_BASE_CLASS_PROPERTIES_H_
