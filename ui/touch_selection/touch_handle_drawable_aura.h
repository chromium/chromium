// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
#define UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/touch_selection/touch_handle.h"
#include "ui/touch_selection/touch_handle_orientation.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace aura {
class Window;
}

namespace ui {

class UI_TOUCH_SELECTION_EXPORT TouchHandleDrawableAura
    : public TouchHandleDrawable,
      public ui::LayerDelegate,
      public ui::NativeThemeObserver {
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

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Window to draw the handle image.
  std::unique_ptr<aura::Window> window_;

  bool enabled_;
  float alpha_;
  ui::TouchHandleOrientation orientation_;

  // Origin position of the handle set via SetOrigin, in coordinate space of
  // selection controller client (i.e. handle's parent).
  gfx::PointF origin_position_;

  // Window bounds relative to the focal position.
  gfx::RectF relative_bounds_;

  // The handle image to draw.
  ui::ImageModel handle_image_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
