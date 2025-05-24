// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_INSETS_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_INSETS_CONVERSIONS_H_

#include "base/component_export.h"

namespace gfx {
class Insets;
class InsetsF;

// Returns an Insets with each component from the input InsetsF floored.
COMPONENT_EXPORT(GEOMETRY) Insets ToFlooredInsets(const InsetsF& insets);

// Returns an Insets with each component from the input InsetsF ceiled.
COMPONENT_EXPORT(GEOMETRY) Insets ToCeiledInsets(const InsetsF& insets);

// Returns a Point with each component from the input PointF rounded.
COMPONENT_EXPORT(GEOMETRY) Insets ToRoundedInsets(const InsetsF& insets);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_INSETS_CONVERSIONS_H_
