// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/display_color_management_mojom_traits.h"

#include "ui/display/mojom/gamma_ramp_rgb_entry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<::display::mojom::GammaCurveDataView, ::display::GammaCurve>::
    Read(::display::mojom::GammaCurveDataView data,
         ::display::GammaCurve* out) {
  std::vector<::display::GammaRampRGBEntry> lut;
  if (!data.ReadLut(&lut)) {
    return false;
  }
  *out = ::display::GammaCurve(lut);
  return true;
}

}  // namespace mojo
