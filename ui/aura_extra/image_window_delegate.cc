// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/image_window_delegate.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace aura_extra {

ImageWindowDelegate::ImageWindowDelegate() = default;

ImageWindowDelegate::~ImageWindowDelegate() = default;

void ImageWindowDelegate::SetImage(const gfx::Image& image) {
  image_ = image;
  if (!window_size_.IsEmpty() && !image_.IsEmpty())
    size_mismatch_ = window_size_ != image_.AsImageSkia().size();
}

gfx::Size ImageWindowDelegate::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size ImageWindowDelegate::GetMaximumSize() const {
  return gfx::Size();
}

void ImageWindowDelegate::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  window_size_ = new_bounds.size();
  if (!image_.IsEmpty())
    size_mismatch_ = window_size_ != image_.AsImageSkia().size();
}

gfx::NativeCursor ImageWindowDelegate::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int ImageWindowDelegate::GetNonClientComponent(const gfx::Point& point) const {
  return HTNOWHERE;
}

bool ImageWindowDelegate::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return false;
}

bool ImageWindowDelegate::CanFocus() {
  return false;
}

void ImageWindowDelegate::OnCaptureLost() {
}

void ImageWindowDelegate::OnPaint(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, window_size_);
  if (background_color_ != SK_ColorTRANSPARENT &&
      (image_.IsEmpty() || size_mismatch_ || !offset_.IsZero())) {
    recorder.canvas()->DrawColor(background_color_);
  }
  if (!image_.IsEmpty()) {
    recorder.canvas()->DrawImageInt(image_.AsImageSkia(), offset_.x(),
                                    offset_.y());
  }
}

void ImageWindowDelegate::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void ImageWindowDelegate::OnWindowDestroying(aura::Window* window) {
}

void ImageWindowDelegate::OnWindowDestroyed(aura::Window* window) {
  delete this;
}

void ImageWindowDelegate::OnWindowTargetVisibilityChanged(bool visible) {
}

bool ImageWindowDelegate::HasHitTestMask() const {
  return false;
}

void ImageWindowDelegate::GetHitTestMask(SkPath* mask) const {}

}  // namespace aura_extra
