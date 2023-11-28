// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_

#include "ui/display/mojom/display_color_management.mojom-shared.h"
#include "ui/display/mojom/gamma_ramp_rgb_entry.mojom-shared.h"

#include "ui/display/types/display_color_management.h"

namespace mojo {

template <>
struct StructTraits<::display::mojom::GammaCurveDataView,
                    ::display::GammaCurve> {
  static const std::vector<::display::GammaRampRGBEntry>& lut(
      const ::display::GammaCurve& gamma_curve) {
    return gamma_curve.lut();
  }

  static bool Read(::display::mojom::GammaCurveDataView data,
                   ::display::GammaCurve* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_
