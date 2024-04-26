// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_paint_util.h"

#include "cc/paint/draw_looper.h"
#include "cc/paint/paint_image_builder.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/switches.h"

namespace gfx {

sk_sp<cc::PaintShader> CreateImageRepShader(const gfx::ImageSkiaRep& image_rep,
                                            SkTileMode tile_mode_x,
                                            SkTileMode tile_mode_y,
                                            const SkMatrix& local_matrix) {
  return CreateImageRepShaderForScale(image_rep, tile_mode_x, tile_mode_y,
                                      local_matrix, image_rep.scale());
}

sk_sp<cc::PaintShader> CreateImageRepShaderForScale(
    const gfx::ImageSkiaRep& image_rep,
    SkTileMode tile_mode_x,
    SkTileMode tile_mode_y,
    const SkMatrix& local_matrix,
    SkScalar scale) {
  // Unscale matrix by |scale| such that the bitmap is drawn at the
  // correct density.
  // Convert skew and translation to pixel coordinates.
  // Thus, for |bitmap_scale| = 2:
  //   x scale = 2, x translation = 1 DIP,
  // should be converted to
  //   x scale = 1, x translation = 2 pixels.
  SkMatrix shader_scale = local_matrix;
  shader_scale.preScale(scale, scale);
  shader_scale.setScaleX(local_matrix.getScaleX() / scale);
  shader_scale.setScaleY(local_matrix.getScaleY() / scale);

  // TODO(malaykeshav): The check for has_paint_image was only added here to
  // prevent generating a paint record in tests. Tests need an instance of
  // base::DiscardableMemoryAllocator to generate the PaintRecord. However most
  // test suites don't have this set.
  // https://crbug.com/891469
  if (!image_rep.has_paint_image()) {
    return cc::PaintShader::MakePaintRecord(
        image_rep.GetPaintRecord(),
        SkRect::MakeIWH(image_rep.pixel_width(), image_rep.pixel_height()),
        tile_mode_x, tile_mode_y, &shader_scale);
  } else {
    return cc::PaintShader::MakeImage(image_rep.paint_image(), tile_mode_x,
                                      tile_mode_y, &shader_scale);
  }
}

sk_sp<cc::PaintShader> CreateGradientShader(const gfx::Point& start_point,
                                            const gfx::Point& end_point,
                                            SkColor start_color,
                                            SkColor end_color) {
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  SkColor4f grad_colors[2] = {SkColor4f::FromColor(start_color),
                              SkColor4f::FromColor(end_color)};
  SkPoint grad_points[2] = {gfx::PointToSkPoint(start_point),
                            gfx::PointToSkPoint(end_point)};

  return cc::PaintShader::MakeLinearGradient(grad_points, grad_colors, nullptr,
                                             2, SkTileMode::kClamp);
}

// This is copied from
// third_party/WebKit/Source/platform/graphics/skia/SkiaUtils.h
static SkScalar RadiusToSigma(double radius) {
  return radius > 0 ? SkDoubleToScalar(0.288675f * radius + 0.5f) : 0;
}

sk_sp<cc::DrawLooper> CreateShadowDrawLooper(
    const std::vector<ShadowValue>& shadows) {
  if (shadows.empty())
    return nullptr;

  cc::DrawLooperBuilder looper_builder;

  looper_builder.AddUnmodifiedContent();  // top layer of the original.

  for (const ShadowValue& shadow : shadows) {
    looper_builder.AddShadow(
        {SkIntToScalar(shadow.x()), SkIntToScalar(shadow.y())},
        RadiusToSigma(shadow.blur() / 2), SkColor4f::FromColor(shadow.color()),
        cc::DrawLooper::kOverrideAlphaFlag);
  }

  return looper_builder.Detach();
}

}  // namespace gfx
