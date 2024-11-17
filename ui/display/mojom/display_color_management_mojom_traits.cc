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
  *out = ::display::GammaCurve(std::move(lut));
  return true;
}

// static
bool StructTraits<::display::mojom::ColorCalibrationDataView,
                  ::display::ColorCalibration>::
    Read(::display::mojom::ColorCalibrationDataView data,
         ::display::ColorCalibration* out) {
  if (!data.ReadSrgbToLinear(&out->srgb_to_linear)) {
    return false;
  }
  if (!data.ReadSrgbToDeviceMatrix(&out->srgb_to_device_matrix)) {
    return false;
  }
  if (!data.ReadLinearToDevice(&out->linear_to_device)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<::display::mojom::ColorTemperatureAdjustmentDataView,
                  ::display::ColorTemperatureAdjustment>::
    Read(::display::mojom::ColorTemperatureAdjustmentDataView data,
         ::display::ColorTemperatureAdjustment* out) {
  if (!data.ReadSrgbMatrix(&out->srgb_matrix)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<::display::mojom::GammaAdjustmentDataView,
                  ::display::GammaAdjustment>::
    Read(::display::mojom::GammaAdjustmentDataView data,
         ::display::GammaAdjustment* out) {
  if (!data.ReadCurve(&out->curve)) {
    return false;
  }
  return true;
}

}  // namespace mojo
