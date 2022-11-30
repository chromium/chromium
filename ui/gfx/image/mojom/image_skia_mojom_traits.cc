// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/mojom/image_skia_mojom_traits.h"

#include <string.h>

#include "base/check_op.h"

namespace mojo {

// static
SkBitmap
StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep>::bitmap(
    const gfx::ImageSkiaRep& input) {
  SkBitmap bitmap = input.GetBitmap();
  DCHECK(!bitmap.drawsNothing());
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);
  return bitmap;
}

// static
float StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep>::scale(
    const gfx::ImageSkiaRep& input) {
  const float scale = input.unscaled() ? 0.0f : input.scale();
  DCHECK_GE(scale, 0.0f);
  return scale;
}

// static
bool StructTraits<gfx::mojom::ImageSkiaRepDataView, gfx::ImageSkiaRep>::Read(
    gfx::mojom::ImageSkiaRepDataView data,
    gfx::ImageSkiaRep* out) {
  // An acceptable scale must be greater than or equal to 0.
  if (data.scale() < 0)
    return false;

  SkBitmap bitmap;
  if (!data.ReadBitmap(&bitmap))
    return false;
  // Null/uninitialized bitmaps are not allowed, and an ImageSkiaRep is never
  // empty-sized either.
  if (bitmap.drawsNothing())
    return false;
  // Similar to BitmapN32, ImageSkiaReps are expected to have an N32 bitmap
  // type.
  if (bitmap.colorType() != kN32_SkColorType)
    return false;

  *out = gfx::ImageSkiaRep(bitmap, data.scale());
  return true;
}

// static
std::vector<gfx::ImageSkiaRep>
StructTraits<gfx::mojom::ImageSkiaDataView, gfx::ImageSkia>::image_reps(
    const gfx::ImageSkia& input) {
  // Trigger the image to load everything.
  input.EnsureRepsForSupportedScales();
  return input.image_reps();
}

// static
bool StructTraits<gfx::mojom::ImageSkiaDataView, gfx::ImageSkia>::Read(
    gfx::mojom::ImageSkiaDataView data,
    gfx::ImageSkia* out) {
  DCHECK(out->isNull());

  std::vector<gfx::ImageSkiaRep> image_reps;
  if (!data.ReadImageReps(&image_reps))
    return false;

  for (const auto& image_rep : image_reps)
    out->AddRepresentation(image_rep);

  if (out->isNull())
    return false;

  out->SetReadOnly();
  return true;
}

}  // namespace mojo
