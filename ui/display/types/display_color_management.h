// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_
#define UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/display/types/display_types_export.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace display {

// A structure that specifies curves for the red, green, and blue color
// channels.
class DISPLAY_TYPES_EXPORT GammaCurve {
 public:
  explicit GammaCurve(std::vector<GammaRampRGBEntry>&& lut);
  GammaCurve();
  GammaCurve(const GammaCurve& other);
  GammaCurve(GammaCurve&& other);
  ~GammaCurve();
  GammaCurve& operator=(const GammaCurve& other);

  // Return a gamma curve that is composed of curve `f` applied after curve
  // `g`.
  static GammaCurve MakeConcat(const GammaCurve& f, const GammaCurve& g);

  // Return a gamma curve with the specified exponent.
  static GammaCurve MakeGamma(float gamma);

  // Return a linear curve that scales each component as specified.
  static GammaCurve MakeScale(float red, float green, float blue);

  // Returns true if this was set to an empty LUT and is therefore the identity
  // function.
  bool IsDefaultIdentity() const { return lut_.empty(); }

  // Evaluate at a point `x` in [0, 1]. If `x` is not in that interval then this
  // function will clamp `x` to [0, 1].
  void Evaluate(float x, uint16_t& r, uint16_t& g, uint16_t& b) const;

  // Evaluate at the specified RGB values. Input values will be clamped to the
  // [0, 1] interval.
  void Evaluate(float rgb[3]) const;

  // Display as a string for debugging.
  std::string ToString() const;

  // Convert to a string for use with action logger tests.
  std::string ToActionString(const std::string& prefix) const;

  // Return the backing lookup table.
  const std::vector<GammaRampRGBEntry>& lut() const { return lut_; }

 private:
  float Evaluate(float x, size_t channel) const;

  // An array of RGB samples that specify a look-up table (LUT) that uniformly
  // samples the unit interval. This may have any number of entries (including
  // 0 which evaluates to identity and 1 which evaluates to a constant).
  std::vector<GammaRampRGBEntry> lut_;

  // A curve to apply before applying `lut_`.
  std::unique_ptr<GammaCurve> pre_curve_;
};

// A structure that contains color calibration information extracted from an
// ICC profile. This is a more general structure than the usual SkColorSpace
// or gfx::ColorSpace that is extracted from an ICC profile in that it allows
// for lookup table based curves and separate curves for each color channel.
// This functionality was added historically (in https://crbug.com/471749 and
// https://crbug.com/495196, for ChromeOS), and may be more information than
// is needed (in which case, the internals of this can be replaced with an
// SkColorSpace or similar).
struct ColorCalibration {
  // The curve that converts from sRGB to sRGB linear space. This does not
  // depend on the monitor and is almost certainly unneeded.
  GammaCurve srgb_to_linear;

  // The matrix that converts from sRGB linear space to the device's linear
  // space.
  skcms_Matrix3x3 srgb_to_device_matrix = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};

  // The curve that converts from the device's linear space to the device's
  // encoding space.
  GammaCurve linear_to_device;
};

// A structure that contains the transformation to make to the display's color
// due to adaptations for color temperature (e.g, https://crbug.com/705816, for
// ChromeOS night light).
struct ColorTemperatureAdjustment {
  // A matrix to apply in sRGB space. In practice, this matrix always diagonal
  // and is applied in the device's color space (not sRGB), though this behavior
  // is subject to change.
  skcms_Matrix3x3 srgb_matrix = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
};

// A structure that contains a per-channel transformation to make in the
// display's color space to adjust the brightness of the screen (e.g,
// b/109942195, for Chromecast gamma adaptation).
struct GammaAdjustment {
  GammaCurve curve;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_
