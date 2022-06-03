// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets.h"

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

std::string Insets::ToString() const {
  // Print members in the same order of the constructor parameters.
  return base::StringPrintf("%d,%d,%d,%d", top(),  left(), bottom(), right());
}

Insets Insets::Offset(const gfx::Vector2d& vector) const {
  return gfx::Insets(base::ClampAdd(top(), vector.y()),
                     base::ClampAdd(left(), vector.x()),
                     base::ClampSub(bottom(), vector.y()),
                     base::ClampSub(right(), vector.x()));
}

Insets ScaleToCeiledInsets(const Insets& insets, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToCeiledInsets(ScaleInsets(gfx::InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToCeiledInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToCeiledInsets(ScaleInsets(gfx::InsetsF(insets), scale));
}

Insets ScaleToFlooredInsets(const Insets& insets,
                            float x_scale,
                            float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToFlooredInsets(ScaleInsets(gfx::InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToFlooredInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToFlooredInsets(ScaleInsets(gfx::InsetsF(insets), scale));
}

Insets ScaleToRoundedInsets(const Insets& insets,
                            float x_scale,
                            float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToRoundedInsets(ScaleInsets(gfx::InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToRoundedInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToRoundedInsets(ScaleInsets(gfx::InsetsF(insets), scale));
}

}  // namespace gfx
