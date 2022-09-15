// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets.h"

#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

Outsets Insets::ToOutsets() const {
  // Conversion from Insets to Outsets negates all components.
  return Outsets()
      .set_left_right(-left(), -right())
      .set_top_bottom(-top(), -bottom());
}

void Insets::Offset(const gfx::Vector2d& vector) {
  set_left_right(base::ClampAdd(left(), vector.x()),
                 base::ClampSub(right(), vector.x()));
  set_top_bottom(base::ClampAdd(top(), vector.y()),
                 base::ClampSub(bottom(), vector.y()));
}

Insets ScaleToCeiledInsets(const Insets& insets, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToCeiledInsets(ScaleInsets(InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToCeiledInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToCeiledInsets(ScaleInsets(InsetsF(insets), scale));
}

Insets ScaleToFlooredInsets(const Insets& insets,
                            float x_scale,
                            float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToFlooredInsets(ScaleInsets(InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToFlooredInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToFlooredInsets(ScaleInsets(InsetsF(insets), scale));
}

Insets ScaleToRoundedInsets(const Insets& insets,
                            float x_scale,
                            float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return insets;
  return ToRoundedInsets(ScaleInsets(InsetsF(insets), x_scale, y_scale));
}

Insets ScaleToRoundedInsets(const Insets& insets, float scale) {
  if (scale == 1.f)
    return insets;
  return ToRoundedInsets(ScaleInsets(InsetsF(insets), scale));
}

}  // namespace gfx
