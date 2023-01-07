// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_style.h"

#include <algorithm>

#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace message_center {

gfx::Size GetImageSizeForContainerSize(const gfx::Size& container_size,
                                       const gfx::Size& image_size) {
  if (container_size.IsEmpty() || image_size.IsEmpty())
    return gfx::Size();

  gfx::Size scaled_size = image_size;
  double proportion =
      scaled_size.height() / static_cast<double>(scaled_size.width());
  // We never want to return an empty image given a non-empty container and
  // image, so round the height to 1.
  scaled_size.SetSize(container_size.width(),
                      std::max(0.5 + container_size.width() * proportion, 1.0));
  if (scaled_size.height() > container_size.height()) {
    scaled_size.SetSize(
        std::max(0.5 + container_size.height() / proportion, 1.0),
        container_size.height());
  }

  return scaled_size;
}

}  // namespace message_center
