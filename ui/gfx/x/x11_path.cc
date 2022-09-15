// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_path.h"

#include <memory>

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace x11 {

std::unique_ptr<std::vector<Rectangle>> CreateRegionFromSkRegion(
    const SkRegion& region) {
  auto result = std::make_unique<std::vector<Rectangle>>();

  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    result->push_back({
        .x = static_cast<int16_t>(i.rect().x()),
        .y = static_cast<int16_t>(i.rect().y()),
        .width = static_cast<uint16_t>(i.rect().width()),
        .height = static_cast<uint16_t>(i.rect().height()),
    });
  }

  return result;
}

std::unique_ptr<std::vector<Rectangle>> CreateRegionFromSkPath(
    const SkPath& path) {
  SkRegion clip{path.getBounds().roundOut()};
  SkRegion region;
  region.setPath(path, clip);
  return CreateRegionFromSkRegion(region);
}

}  // namespace x11
