// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_PAINT_UTIL_H_
#define UI_GFX_SKIA_PAINT_UTIL_H_

#include <memory>
#include <vector>

#include "cc/paint/paint_shader.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/shadow_value.h"

class SkDrawLooper;
class SkMatrix;

namespace gfx {

class ImageSkiaRep;

// Creates a bitmap shader for the image rep with the image rep's scale factor.
// Sets the created shader's local matrix such that it displays the image rep at
// the correct scale factor.
// The shader's local matrix should not be changed after the shader is created.
// TODO(pkotwicz): Allow shader's local matrix to be changed after the shader
// is created.
//
GFX_EXPORT sk_sp<cc::PaintShader> CreateImageRepShader(
    const gfx::ImageSkiaRep& image_rep,
    SkTileMode tile_mode_x,
    SkTileMode tile_mode_y,
    const SkMatrix& local_matrix);

// Creates a bitmap shader for the image rep with the passed in scale factor.
GFX_EXPORT sk_sp<cc::PaintShader> CreateImageRepShaderForScale(
    const gfx::ImageSkiaRep& image_rep,
    SkTileMode tile_mode_x,
    SkTileMode tile_mode_y,
    const SkMatrix& local_matrix,
    SkScalar scale);

// Creates a gradient shader. The caller owns the shader.
GFX_EXPORT sk_sp<cc::PaintShader> CreateGradientShader(
    const gfx::Point& start_point,
    const gfx::Point& end_point,
    SkColor start_color,
    SkColor end_color);

// Creates a draw looper to generate |shadows|. The caller owns the draw looper.
// NULL is returned if |shadows| is empty since no draw looper is needed in
// this case.
GFX_EXPORT sk_sp<SkDrawLooper> CreateShadowDrawLooper(
    const std::vector<ShadowValue>& shadows);

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTIL_H_
