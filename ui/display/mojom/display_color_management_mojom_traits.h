// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_
#define UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolorspace_mojom_traits.h"
#include "ui/display/mojom/display_color_management.mojom-shared.h"
#include "ui/display/mojom/gamma_ramp_rgb_entry.mojom-shared.h"
#include "ui/display/types/display_color_management.h"

namespace mojo {

// GammaCurve
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

// ColorCalibration
template <>
struct StructTraits<::display::mojom::ColorCalibrationDataView,
                    ::display::ColorCalibration> {
  static const ::display::GammaCurve& srgb_to_linear(
      const ::display::ColorCalibration& in) {
    return in.srgb_to_linear;
  }
  static const ::skcms_Matrix3x3& srgb_to_device_matrix(
      const ::display::ColorCalibration& in) {
    return in.srgb_to_device_matrix;
  }
  static const ::display::GammaCurve& linear_to_device(
      const ::display::ColorCalibration& in) {
    return in.linear_to_device;
  }

  static bool Read(::display::mojom::ColorCalibrationDataView data,
                   ::display::ColorCalibration* out);
};

// ColorTemperatureAdjustment
template <>
struct StructTraits<::display::mojom::ColorTemperatureAdjustmentDataView,
                    ::display::ColorTemperatureAdjustment> {
  static const ::skcms_Matrix3x3& srgb_matrix(
      const ::display::ColorTemperatureAdjustment& in) {
    return in.srgb_matrix;
  }

  static bool Read(::display::mojom::ColorTemperatureAdjustmentDataView data,
                   ::display::ColorTemperatureAdjustment* out);
};

// GammaAdjustment
template <>
struct StructTraits<::display::mojom::GammaAdjustmentDataView,
                    ::display::GammaAdjustment> {
  static const ::display::GammaCurve& curve(
      const ::display::GammaAdjustment& in) {
    return in.curve;
  }

  static bool Read(::display::mojom::GammaAdjustmentDataView data,
                   ::display::GammaAdjustment* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJOM_DISPLAY_COLOR_MANAGEMENT_MOJOM_TRAITS_H_
