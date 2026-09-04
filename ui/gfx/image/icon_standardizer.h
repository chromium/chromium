// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_ICON_STANDARDIZER_H_
#define UI_GFX_IMAGE_ICON_STANDARDIZER_H_

#include <optional>

#include "base/component_export.h"

namespace gfx {

class ImageSkia;
class ImageSkiaRep;

// Takes an icon image and returns a standardized version of that icon. This
// function consists of the following steps:
// 1. Check if the original icon is already circle shaped. If it is, then
//    return the original input icon.
// 2. Find the scale required to resize and fit the original icon inside of
//    a new circle background.
// 3. Scale down the icon and draw it over a background circle. Return the newly
//    generated icon as the standard icon.
COMPONENT_EXPORT(GFX)
ImageSkia CreateStandardAppIconImage(const ImageSkia& image);

// The same as CreateStandardAppIconImage but for ImageSkiaRep.
// Returns nullopt if base_rep was not modified.
COMPONENT_EXPORT(GFX)
std::optional<ImageSkiaRep> CreateStandardAppIconImageRep(
    const ImageSkiaRep& base_rep,
    float scale);

}  // namespace gfx

#endif  // UI_GFX_IMAGE_ICON_STANDARDIZER_H_
