// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/nine_image_painter_factory.h"

#include <stddef.h>

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/nine_image_painter.h"

namespace ui {

namespace {

std::vector<gfx::ImageSkia> ImageIdsToImages(const int image_ids[]) {
  DCHECK(image_ids);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  std::vector<gfx::ImageSkia> images(9);
  for (size_t i = 0; i < 9; ++i) {
    if (image_ids[i] != 0)
      images[i] = *rb.GetImageSkiaNamed(image_ids[i]);
  }
  return images;
}

}  // namespace

std::unique_ptr<gfx::NineImagePainter> CreateNineImagePainter(
    const int image_ids[]) {
  return std::make_unique<gfx::NineImagePainter>(ImageIdsToImages(image_ids));
}

}  // namespace ui
