// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/nine_patch_resource.h"

#include "cc/layers/nine_patch_layer.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// static
NinePatchResource* NinePatchResource::From(Resource* resource) {
  DCHECK_EQ(Type::NINE_PATCH_BITMAP, resource->type());
  return static_cast<NinePatchResource*>(resource);
}

NinePatchResource::NinePatchResource(gfx::Rect padding, gfx::Rect aperture)
    : Resource(Type::NINE_PATCH_BITMAP),
      padding_(padding),
      aperture_(aperture) {}

NinePatchResource::~NinePatchResource() = default;

gfx::Size NinePatchResource::DrawSize(const gfx::Size& content_size) const {
  // The effective drawing size of the resource includes the size of the content
  // (fit inside the expanded padding area) and the size of the margins on each
  // side.
  return gfx::Size(content_size.width() + size().width() - padding_.width(),
                   content_size.height() + size().height() - padding_.height());
}

gfx::PointF NinePatchResource::DrawPosition(
    const gfx::Point& content_position) const {
  // Offset the location of the layer by the amount taken by the left and top
  // margin.
  return gfx::PointF(content_position.x() - padding_.x(),
                     content_position.y() - padding_.y());
}

gfx::Rect NinePatchResource::Border(const gfx::Size& bounds) const {
  return Border(bounds, gfx::InsetsF(1.f, 1.f, 1.f, 1.f));
}

gfx::Rect NinePatchResource::Border(const gfx::Size& bounds,
                                    const gfx::InsetsF& scale) const {
  // Calculate whether or not we need to scale down the border if the bounds of
  // the layer are going to be smaller than the aperture padding.
  float x_scale = std::min((float)bounds.width() / size().width(), 1.f);
  float y_scale = std::min((float)bounds.height() / size().height(), 1.f);

  float left_scale = std::min(x_scale * scale.left(), 1.f);
  float right_scale = std::min(x_scale * scale.right(), 1.f);
  float top_scale = std::min(y_scale * scale.top(), 1.f);
  float bottom_scale = std::min(y_scale * scale.bottom(), 1.f);

  return gfx::Rect(aperture_.x() * left_scale, aperture_.y() * top_scale,
                   (size().width() - aperture_.width()) * right_scale,
                   (size().height() - aperture_.height()) * bottom_scale);
}

std::unique_ptr<Resource> NinePatchResource::CreateForCopy() {
  return std::make_unique<NinePatchResource>(padding_, aperture_);
}

}  // namespace ui
