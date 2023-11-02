// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_IMAGE_WINDOW_DELEGATE_H_
#define UI_AURA_EXTRA_IMAGE_WINDOW_DELEGATE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura_extra/aura_extra_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"

namespace aura_extra {

// An ImageWindowDelegate paints an image for a Window. If there is uncovered
// area, it also fills the window with a background color when specified.
// The delegate does not consume any event.
//
// The delegate destroys itself when the Window is destroyed. This is done in
// |OnWindowDestroyed()| function which subclasses can override to prevent
// self-destroying.
class AURA_EXTRA_EXPORT ImageWindowDelegate : public aura::WindowDelegate {
 public:
  ImageWindowDelegate();
  ImageWindowDelegate(const ImageWindowDelegate&) = delete;
  ImageWindowDelegate& operator=(const ImageWindowDelegate&) = delete;

  void SetImage(const gfx::Image& image);

  void set_background_color(SkColor color) { background_color_ = color; }
  void set_image_offset(const gfx::Vector2d& offset) { offset_ = offset; }

  bool has_image() const { return !image_.IsEmpty(); }

 protected:
  ~ImageWindowDelegate() override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;

 protected:
  SkColor background_color_ = SK_ColorTRANSPARENT;
  gfx::Image image_;
  gfx::Vector2d offset_;

  gfx::Size window_size_;

  // Keeps track of whether the window size matches the image size or not. If
  // the image size is smaller than the window size, then the delegate fills the
  // missing regions with |background_color_| (default is transparent).
  bool size_mismatch_ = false;
};

}  // namespace aura_extra

#endif  // UI_AURA_EXTRA_IMAGE_WINDOW_DELEGATE_H_
