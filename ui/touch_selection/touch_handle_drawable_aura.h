// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
#define UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_

#include "base/memory/raw_ptr.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace aura {
class Window;
}

namespace aura_extra {
class ImageWindowDelegate;
}

namespace ui {

class UI_TOUCH_SELECTION_EXPORT TouchHandleDrawableAura
    : public TouchHandleDrawable {
 public:
  explicit TouchHandleDrawableAura(aura::Window* parent);

  TouchHandleDrawableAura(const TouchHandleDrawableAura&) = delete;
  TouchHandleDrawableAura& operator=(const TouchHandleDrawableAura&) = delete;

  ~TouchHandleDrawableAura() override;

 private:
  void UpdateBounds();

  bool IsVisible() const;

  // TouchHandleDrawable:
  void SetEnabled(bool enabled) override;
  void SetOrientation(TouchHandleOrientation orientation,
                      bool mirror_vertical,
                      bool mirror_horizontal) override;
  void SetOrigin(const gfx::PointF& position) override;
  void SetAlpha(float alpha) override;
  gfx::RectF GetVisibleBounds() const override;
  float GetDrawableHorizontalPaddingRatio() const override;

  std::unique_ptr<aura::Window> window_;
  // `window_delegate_` self destroys when`OnWindowDestroyed()` is invoked
  // during the destruction of `window_`. It must be declared last and cleared
  // first to avoid holding a ptr to freed memory.
  raw_ptr<aura_extra::ImageWindowDelegate> window_delegate_;
  bool enabled_;
  float alpha_;
  ui::TouchHandleOrientation orientation_;

  // Origin position of the handle set via SetOrigin, in coordinate space of
  // selection controller client (i.e. handle's parent).
  gfx::PointF origin_position_;

  // Window bounds relative to the focal position.
  gfx::RectF relative_bounds_;
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
