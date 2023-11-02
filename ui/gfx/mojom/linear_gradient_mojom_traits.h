// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_LINEAR_GRADIENT_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_LINEAR_GRADIENT_MOJOM_TRAITS_H_

#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/mojom/linear_gradient.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::StepDataView, gfx::LinearGradient::Step> {
  static float fraction(const gfx::LinearGradient::Step& input) {
    return input.fraction;
  }

  static uint8_t alpha(const gfx::LinearGradient::Step& input) {
    return input.alpha;
  }

  static bool Read(gfx::mojom::StepDataView data,
                   gfx::LinearGradient::Step* out) {
    out->fraction = data.fraction();
    out->alpha = data.alpha();
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::LinearGradientDataView, gfx::LinearGradient> {
  static int16_t angle(const gfx::LinearGradient& input) {
    return input.angle();
  }

  static uint8_t step_count(const gfx::LinearGradient& input) {
    return input.step_count();
  }

  static const gfx::LinearGradient::StepArray& steps(
      const gfx::LinearGradient& input) {
    return input.steps();
  }

  static bool Read(gfx::mojom::LinearGradientDataView data,
                   gfx::LinearGradient* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_LINEAR_GRADIENT_MOJOM_TRAITS_H_
