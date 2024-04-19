// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATION_DELEGATE_H_
#define UI_COMPOSITOR_LAYER_ANIMATION_DELEGATE_H_

#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {
class Layer;
}

namespace ui {

class Layer;
class LayerAnimatorCollection;
class LayerThreadedAnimationDelegate;

// Layer animations interact with the layers using this interface.
class COMPOSITOR_EXPORT LayerAnimationDelegate {
 public:
  virtual void SetBoundsFromAnimation(const gfx::Rect& bounds,
                                      PropertyChangeReason reason) = 0;
  virtual void SetTransformFromAnimation(const gfx::Transform& transform,
                                         PropertyChangeReason reason) = 0;
  virtual void SetOpacityFromAnimation(float opacity,
                                       PropertyChangeReason reason) = 0;
  virtual void SetVisibilityFromAnimation(bool visibility,
                                          PropertyChangeReason reason) = 0;
  virtual void SetBrightnessFromAnimation(float brightnes,
                                          PropertyChangeReason reasons) = 0;
  virtual void SetGrayscaleFromAnimation(float grayscale,
                                         PropertyChangeReason reason) = 0;
  virtual void SetColorFromAnimation(SkColor color,
                                     PropertyChangeReason reason) = 0;
  virtual void SetClipRectFromAnimation(const gfx::Rect& clip_rect,
                                        PropertyChangeReason reason) = 0;
  virtual void SetRoundedCornersFromAnimation(
      const gfx::RoundedCornersF& rounded_corners,
      PropertyChangeReason reason) = 0;
  virtual void SetGradientMaskFromAnimation(
      const gfx::LinearGradient& gradient_mask,
      PropertyChangeReason reason) = 0;
  virtual void ScheduleDrawForAnimation() = 0;
  virtual const gfx::Rect& GetBoundsForAnimation() const = 0;
  virtual gfx::Transform GetTransformForAnimation() const = 0;
  virtual float GetOpacityForAnimation() const = 0;
  virtual bool GetVisibilityForAnimation() const = 0;
  virtual float GetBrightnessForAnimation() const = 0;
  virtual float GetGrayscaleForAnimation() const = 0;
  virtual SkColor GetColorForAnimation() const = 0;
  virtual gfx::Rect GetClipRectForAnimation() const = 0;
  virtual gfx::RoundedCornersF GetRoundedCornersForAnimation() const = 0;
  virtual const gfx::LinearGradient& GetGradientMaskForAnimation() const = 0;
  virtual float GetDeviceScaleFactor() const = 0;
  virtual ui::Layer* GetLayer() = 0;
  virtual cc::Layer* GetCcLayer() const = 0;
  virtual LayerAnimatorCollection* GetLayerAnimatorCollection() = 0;
  virtual LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() = 0;
  virtual float GetRefreshRate() const = 0;

 protected:
  virtual ~LayerAnimationDelegate() {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATION_DELEGATE_H_
