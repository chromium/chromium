// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_

#include <string>
#include <vector>

#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry_skia_export.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/vulkan/include/vulkan/vulkan.h"
#endif

class SkBitmap;
class SkMatrix;

namespace gfx {

class Point;
class PointF;
class Rect;
class RectF;
class Transform;

// Convert between Skia and gfx types.
GEOMETRY_SKIA_EXPORT SkPoint PointToSkPoint(const Point& point);
GEOMETRY_SKIA_EXPORT SkIPoint PointToSkIPoint(const Point& point);
GEOMETRY_SKIA_EXPORT SkPoint PointFToSkPoint(const PointF& point);
GEOMETRY_SKIA_EXPORT SkRect RectToSkRect(const Rect& rect);
GEOMETRY_SKIA_EXPORT SkIRect RectToSkIRect(const Rect& rect);
GEOMETRY_SKIA_EXPORT Rect SkIRectToRect(const SkIRect& rect);
GEOMETRY_SKIA_EXPORT SkRect RectFToSkRect(const RectF& rect);
GEOMETRY_SKIA_EXPORT RectF SkRectToRectF(const SkRect& rect);
GEOMETRY_SKIA_EXPORT SkSize SizeFToSkSize(const SizeF& size);
GEOMETRY_SKIA_EXPORT SkISize SizeToSkISize(const Size& size);
GEOMETRY_SKIA_EXPORT SizeF SkSizeToSizeF(const SkSize& size);
GEOMETRY_SKIA_EXPORT Size SkISizeToSize(const SkISize& size);

GEOMETRY_SKIA_EXPORT void QuadFToSkPoints(const gfx::QuadF& quad,
                                          SkPoint points[4]);

GEOMETRY_SKIA_EXPORT void TransformToFlattenedSkMatrix(
    const gfx::Transform& transform,
    SkMatrix* flattened);

// Returns true if the two bitmaps contain the same pixels.
GEOMETRY_SKIA_EXPORT bool BitmapsAreEqual(const SkBitmap& bitmap1,
                                          const SkBitmap& bitmap2);

// Converts Skia ARGB format pixels in |skia| to RGBA.
GEOMETRY_SKIA_EXPORT void ConvertSkiaToRGBA(const unsigned char* skia,
                                            int pixel_width,
                                            unsigned char* rgba);

// Converts a Skia floating-point value to an int appropriate for hb_position_t.
GEOMETRY_SKIA_EXPORT int SkiaScalarToHarfBuzzUnits(SkScalar value);

// Converts an hb_position_t to a Skia floating-point value.
GEOMETRY_SKIA_EXPORT SkScalar HarfBuzzUnitsToSkiaScalar(int value);

// Converts an hb_position_t to a float.
GEOMETRY_SKIA_EXPORT float HarfBuzzUnitsToFloat(int value);

#if BUILDFLAG(ENABLE_VULKAN)
// Converts a Skia color type to a compitable VkFormat
GEOMETRY_SKIA_EXPORT VkFormat SkColorTypeToVkFormat(SkColorType color_type);
#endif

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTIL_H_
