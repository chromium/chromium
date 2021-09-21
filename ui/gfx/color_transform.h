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
  struct Options {
    // Used in testing to verify that optimizations have no effect.
    bool disable_optimizations = false;

    // Used to adjust the transfer and range adjust matrices.
    uint32_t src_bit_depth = kDefaultBitDepth;
    uint32_t dst_bit_depth = kDefaultBitDepth;
  };

  // TriStimulus is a color coordinate in any color space.
  // Channel order is XYZ, RGB or YUV.
  typedef Point3F TriStim;

  ColorTransform();

  ColorTransform(const ColorTransform&) = delete;
  ColorTransform& operator=(const ColorTransform&) = delete;

  virtual ~ColorTransform();
  virtual gfx::ColorSpace GetSrcColorSpace() const = 0;
  virtual gfx::ColorSpace GetDstColorSpace() const = 0;

  // Perform transformation of colors, |colors| is both input and output.
  virtual void Transform(TriStim* colors, size_t num) const = 0;

  // Return GLSL shader source that defines a function DoColorConversion that
  // converts a vec3 according to this transform.
  virtual std::string GetShaderSource() const = 0;

  // Return SKSL shader sources that modifies an "inout half4 color" according
  // to this transform. Input and output are non-premultiplied alpha.
  virtual std::string GetSkShaderSource() const = 0;

  // Returns true if this transform is the identity.
  virtual bool IsIdentity() const = 0;

  virtual size_t NumberOfStepsForTesting() const = 0;

  // Two special cases:
  // 1. If no source color space is specified (i.e., src.IsValid() is false), do
  // no transformation.
  // 2. If the target color space is not defined (i.e., dst.IsValid() is false),
  // just apply the range adjust and inverse transfer matrices. This can be used
  // for YUV to RGB color conversion.
  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& src,
      const ColorSpace& dst);

  static std::unique_ptr<ColorTransform> NewColorTransform(
      const ColorSpace& src,
      const ColorSpace& dst,
      const Options& options);

 private:
  // The default bit depth assumed by NewColorTransform().
  static constexpr int kDefaultBitDepth = 8;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_TRANSFORM_H_
