// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_
#define UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_

#include "ui/android/resources/resource.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
class NinePatchLayer;
}

namespace ui {

class UI_ANDROID_EXPORT NinePatchResource final : public Resource {
 public:
  static NinePatchResource* From(Resource* resource);

  NinePatchResource(gfx::Rect padding, gfx::Rect aperture);
  ~NinePatchResource() override;

  std::unique_ptr<Resource> CreateForCopy() override;

  // Returns the drawing size that the resource will take for padding content
  // of size |content_size|.
  gfx::Size DrawSize(const gfx::Size& content_size) const;

  // Returns the position where the resource should be drawn to account for
  // margins, given the |content_position| in the parent's coordinate space.
  gfx::PointF DrawPosition(const gfx::Point& content_position) const;

  // Updates draw properties on |layer| used to draw this resource. The
  // |content_location| is the rect of the content to be fit inside the resource
  // in the parent's coordinate space.
  void UpdateNinePatchLayer(cc::NinePatchLayer* layer,
                            const gfx::Rect& content_location) const;

  gfx::Rect Border(const gfx::Size& bounds) const;
  gfx::Rect Border(const gfx::Size& bounds, const gfx::InsetsF& scale) const;

  gfx::Rect padding() const { return padding_; }
  gfx::Rect aperture() const { return aperture_; }

 private:
  const gfx::Rect padding_;
  const gfx::Rect aperture_;
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_NINE_PATCH_RESOURCE_H_
