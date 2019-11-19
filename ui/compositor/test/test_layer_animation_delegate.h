// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_
#define UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "cc/layers/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_threaded_animation_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

class TestLayerThreadedAnimationDelegate
    : public LayerThreadedAnimationDelegate {
 public:
  TestLayerThreadedAnimationDelegate();
  ~TestLayerThreadedAnimationDelegate() override;

  // Implementation of LayerThreadedAnimationDelegate
  void AddThreadedAnimation(
      std::unique_ptr<cc::KeyframeModel> keyframe_model) override;
  void RemoveThreadedAnimation(int keyframe_model_id) override;
};

class TestLayerAnimationDelegate : public LayerAnimationDelegate {
 public:
  TestLayerAnimationDelegate();
  explicit TestLayerAnimationDelegate(const LayerAnimationDelegate& other);
  TestLayerAnimationDelegate(const TestLayerAnimationDelegate& other);
  ~TestLayerAnimationDelegate() override;

  // Expects the last PropertyChangeReason to be unset.
  void ExpectLastPropertyChangeReasonIsUnset();

  // Expects the last PropertyChangeReason to be |reason|. Then, unsets the last
  // PropertyChangeReason.
  void ExpectLastPropertyChangeReason(PropertyChangeReason reason);

  // Sets the current frame number to be returned by GetFrameNumber. This can be
  // used to simulate receiving acks of frame submission, in order to test
  // advancing of animations.
  void SetFrameNumber(int frame_number);

  // Implementation of LayerAnimationDelegate
  void SetBoundsFromAnimation(const gfx::Rect& bounds,
                              PropertyChangeReason reason) override;
  void SetTransformFromAnimation(const gfx::Transform& transform,
                                 PropertyChangeReason reason) override;
  void SetOpacityFromAnimation(float opacity,
                               PropertyChangeReason reason) override;
  void SetVisibilityFromAnimation(bool visibility,
                                  PropertyChangeReason reason) override;
  void SetBrightnessFromAnimation(float brightness,
                                  PropertyChangeReason reason) override;
  void SetGrayscaleFromAnimation(float grayscale,
                                 PropertyChangeReason reason) override;
  void SetColorFromAnimation(SkColor color,
                             PropertyChangeReason reason) override;
  void SetClipRectFromAnimation(const gfx::Rect& clip_rect,
                                PropertyChangeReason reason) override;
  void SetRoundedCornersFromAnimation(
      const gfx::RoundedCornersF& rounded_corners,
      PropertyChangeReason reason) override;
  void ScheduleDrawForAnimation() override;
  const gfx::Rect& GetBoundsForAnimation() const override;
  gfx::Transform GetTransformForAnimation() const override;
  float GetOpacityForAnimation() const override;
  bool GetVisibilityForAnimation() const override;
  float GetBrightnessForAnimation() const override;
  float GetGrayscaleForAnimation() const override;
  SkColor GetColorForAnimation() const override;
  gfx::Rect GetClipRectForAnimation() const override;
  gfx::RoundedCornersF GetRoundedCornersForAnimation() const override;
  float GetDeviceScaleFactor() const override;
  LayerAnimatorCollection* GetLayerAnimatorCollection() override;
  ui::Layer* GetLayer() override;
  cc::Layer* GetCcLayer() const override;
  LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() override;
  int GetFrameNumber() const override;
  float GetRefreshRate() const override;

 private:
  void CreateCcLayer();

  TestLayerThreadedAnimationDelegate threaded_delegate_;

  bool last_property_change_reason_is_set_ = false;
  PropertyChangeReason last_property_change_reason_ =
      PropertyChangeReason::NOT_FROM_ANIMATION;

  gfx::Rect bounds_;
  gfx::Transform transform_;
  float opacity_;
  bool visibility_;
  float brightness_;
  float grayscale_;
  SkColor color_;
  gfx::Rect clip_rect_;
  gfx::RoundedCornersF rounded_corners_;
  scoped_refptr<cc::Layer> cc_layer_;
  int frame_number_ = 0;

  // Allow copy and assign.
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_DELEGATE_H_
