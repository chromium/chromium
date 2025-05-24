// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/class_properties.h"

#include <stdint.h>

#include <concepts>
#include <string>

#include "base/component_export.h"
#include "ui/base/class_property.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), bool)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), float)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE), int)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_BASE),
                                       std::u16string*)

// Some classes store `int32_t` properties. This specialization is not defined
// above because `int32_t` is a type alias for `int` on the platforms Chromium
// supports.
static_assert(std::same_as<int, int32_t>);
