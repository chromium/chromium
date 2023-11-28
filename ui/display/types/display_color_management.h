// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_
#define UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_

#include <vector>

#include "ui/display/types/display_types_export.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"

namespace display {

// A structure that specifies curves for the red, green, and blue color
// channels.
class DISPLAY_TYPES_EXPORT GammaCurve {
 public:
  explicit GammaCurve(const std::vector<GammaRampRGBEntry>& lut);
  GammaCurve();
  GammaCurve(const GammaCurve& other);
  GammaCurve(GammaCurve&& other);
  ~GammaCurve();
  GammaCurve& operator=(const GammaCurve& other);

  // Returns true if this was set to an empty LUT and is therefore the identity
  // function.
  bool IsDefaultIdentity() const { return lut_.empty(); }

  // Evaluate at a point `x` in [0, 1]. If `x` is not in that interval then this
  // function will clamp `x` to [0, 1].
  void Evaluate(float x, uint16_t& r, uint16_t& g, uint16_t& b) const;

  // Display as a string for debugging.
  std::string ToString() const;

  // Convert to a string for use with action logger tests.
  std::string ToActionString(const std::string& prefix) const;

  // Return the backing lookup table.
  const std::vector<GammaRampRGBEntry>& lut() const { return lut_; }

 private:
  // An array of RGB samples that specify a look-up table (LUT) that uniformly
  // samples the unit interval. This may have any number of entries (including
  // 0 which evaluates to identity and 1 which evaluates to a constant).
  std::vector<GammaRampRGBEntry> lut_;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_COLOR_MANAGEMENT_H_
