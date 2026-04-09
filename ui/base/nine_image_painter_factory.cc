// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/nine_image_painter_factory.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/nine_image_painter.h"

namespace ui {

namespace {

std::vector<gfx::ImageSkia> ImageIdsToImages(const NineImageIds& image_ids) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  std::vector<gfx::ImageSkia> images(gfx::kNineImageCount);
  for (size_t i = 0; i < gfx::kNineImageCount; ++i) {
    if (image_ids[i] != 0) {
      images[i] = *rb.GetImageSkiaNamed(image_ids[i]);
    }
  }
  return images;
}

}  // namespace

std::unique_ptr<gfx::NineImagePainter> CreateNineImagePainter(
    const NineImageIds& image_ids) {
  return std::make_unique<gfx::NineImagePainter>(ImageIdsToImages(image_ids));
}

}  // namespace ui
