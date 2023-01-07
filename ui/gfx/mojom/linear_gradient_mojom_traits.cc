// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/linear_gradient_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::LinearGradientDataView, gfx::LinearGradient>::
    Read(gfx::mojom::LinearGradientDataView data, gfx::LinearGradient* out) {
  std::array<gfx::LinearGradient::Step, gfx::LinearGradient::kMaxStepSize>
      steps_data;
  if (!data.ReadSteps(&steps_data))
    return false;

  if (data.step_count() > steps_data.size())
    return false;

  for (int i = 0; i < data.step_count(); ++i) {
    out->AddStep(steps_data[i].fraction, steps_data[i].alpha);
  }
  out->set_angle(data.angle());

  return true;
}

}  // namespace mojo
