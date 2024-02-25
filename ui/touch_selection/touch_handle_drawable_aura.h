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
      public LayerDelegate,
      public NativeThemeObserver {
 public:
  explicit TouchHandleDrawableAura(aura::Window* parent);

  TouchHandleDrawableAura(const TouchHandleDrawableAura&) = delete;
  TouchHandleDrawableAura& operator=(const TouchHandleDrawableAura&) = delete;

  ~TouchHandleDrawableAura() override;

 private:
  // Updates the bounds of the window containing the handle image.
  void UpdateWindowBounds();

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

  // LayerDelegate:
  void OnPaintLayer(const PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // NativeThemeObserver:
  void OnNativeThemeUpdated(NativeTheme* observed_theme) override;

  // The window for drawing the handle image. This doesn't include invisible
  // padding which is applied around the handle image.
  std::unique_ptr<aura::Window> window_;

  bool enabled_;

  // Used to set the opacity of the handle drawable. The actual handle opacity
  // is further scaled by a max opacity value (since the handle can be slightly
  // transparent by default).
  float alpha_;
  TouchHandleOrientation orientation_;

  // The origin of the targetable area of the touch handle, in coordinates of
  // the handle window's parent. When drawing the handle image, an additional
  // offset should be applied to this origin to account for invisible padding.
  gfx::PointF targetable_origin_;

  // The handle image to draw.
  ImageModel handle_image_;

  base::ScopedObservation<NativeTheme, NativeThemeObserver> theme_observation_{
      this};
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_DRAWABLE_AURA_H_
