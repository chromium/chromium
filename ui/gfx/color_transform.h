// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_TRANSFORM_H_
#define UI_GFX_COLOR_TRANSFORM_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT ColorTransform {
 public:
  enum class Intent { INTENT_ABSOLUTE, INTENT_PERCEPTUAL, TEST_NO_OPT };

  // TriStimulus is a color coordinate in any color space.
  // Channel order is XYZ, RGB or YUV.
  typedef Point3F TriStim;

  ColorTransform();
  virtual ~ColorTransform();
  virtual gfx::ColorSpace GetSrcColorSpace() const = 0;
  virtual gfx::ColorSpace GetDstColorSpace() const = 0;

  // Perform transformation of colors, |colors| is both input and output.
  virtual void Transform(TriStim* colors, size_t num) const = 0;

  // Return GLSL shader source that defines a function DoColorConversion that
  // converts a vec3 according to this transform.
  virtual bool CanGetShaderSource() const = 0;
  virtual std::string GetShaderSource() const = 0;

  // Return SKSL shader sources that modifies an "inout half4 color" according
  // to this transform. Input and output are non-premultiplied alpha.
  virtual std::string GetSkShaderSource() const = 0;

  // Returns true if this transform is the identity.
  virtual bool IsIdentity() const = 0;

  virtual size_t NumberOfStepsForTesting() const = 0;

  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& from,
      const ColorSpace& to,
      Intent intent);

 private:
  DISALLOW_COPY_AND_ASSIGN(ColorTransform);
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_TRANSFORM_H_
