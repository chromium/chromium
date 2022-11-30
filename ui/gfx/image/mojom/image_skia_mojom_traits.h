// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_MOJOM_IMAGE_SKIA_MOJOM_TRAITS_H_
#define UI_GFX_IMAGE_MOJOM_IMAGE_SKIA_MOJOM_TRAITS_H_

#include <stdint.h>

#include <vector>

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/mojom/image.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep> {
  static SkBitmap bitmap(const gfx::ImageSkiaRep& input);
  static float scale(const gfx::ImageSkiaRep& input);

  static bool Read(gfx::mojom::ImageSkiaRepDataView data,
                   gfx::ImageSkiaRep* out);
};

template <>
struct StructTraits<gfx::mojom::ImageSkiaDataView, gfx::ImageSkia> {
  static std::vector<gfx::ImageSkiaRep> image_reps(const gfx::ImageSkia& input);

  static bool IsNull(const gfx::ImageSkia& input) { return input.isNull(); }
  static void SetToNull(gfx::ImageSkia* out) { *out = gfx::ImageSkia(); }

  static bool Read(gfx::mojom::ImageSkiaDataView data, gfx::ImageSkia* out);
};

}  // namespace mojo

#endif  // UI_GFX_IMAGE_MOJOM_IMAGE_SKIA_MOJOM_TRAITS_H_
