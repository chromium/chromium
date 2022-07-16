// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IMAGE_MODEL_UTILS_H_
#define UI_VIEWS_IMAGE_MODEL_UTILS_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/views_export.h"

namespace ui {
class ColorProvider;
class ImageModel;
}  // namespace ui

namespace views {

// Returns an ImageSkia representation from an ImageModel representation.
// `color_provider` must be non-null if `model` represents a vector icon. If
// `model` is empty, it returns an empty ImageSkia.
VIEWS_EXPORT gfx::ImageSkia GetImageSkiaFromImageModel(
    const ui::ImageModel& model,
    const ui::ColorProvider* color_provider = nullptr);
}  // namespace views

#endif  // UI_VIEWS_IMAGE_MODEL_UTILS_H_
