// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_layer_animation_delegate.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"

namespace ui {

TestLayerThreadedAnimationDelegate::TestLayerThreadedAnimationDelegate() {}

TestLayerThreadedAnimationDelegate::~TestLayerThreadedAnimationDelegate() {}

TestLayerAnimationDelegate::TestLayerAnimationDelegate()
    : opacity_(1.0f),
      visibility_(true),
      brightness_(0.0f),
      grayscale_(0.0f),
      color_(SK_ColorBLACK) {
  CreateCcLayer();
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const LayerAnimationDelegate& other)
    : bounds_(other.GetBoundsForAnimation()),
      transform_(other.GetTransformForAnimation()),
      opacity_(other.GetOpacityForAnimation()),
      visibility_(other.GetVisibilityForAnimation()),
      color_(SK_ColorBLACK) {
  CreateCcLayer();
}

TestLayerAnimationDelegate::TestLayerAnimationDelegate(
    const TestLayerAnimationDelegate& other) = default;

TestLayerAnimationDelegate::~TestLayerAnimationDelegate() {
}

void TestLayerAnimationDelegate::ExpectLastPropertyChangeReasonIsUnset() {
  EXPECT_FALSE(last_property_change_reason_is_set_);
}

void TestLayerAnimationDelegate::ExpectLastPropertyChangeReason(
    PropertyChangeReason reason) {
  EXPECT_TRUE(last_property_change_reason_is_set_);
  EXPECT_EQ(last_property_change_reason_, reason);
  last_property_change_reason_is_set_ = false;
}

void TestLayerAnimationDelegate::SetFrameNumber(int frame_number) {
  frame_number_ = frame_number;
}

void TestLayerAnimationDelegate::SetBoundsFromAnimation(
    const gfx::Rect& bounds,
    PropertyChangeReason reason) {
  bounds_ = bounds;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetTransformFromAnimation(
    const gfx::Transform& transform,
    PropertyChangeReason reason) {
  transform_ = transform;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetOpacityFromAnimation(
    float opacity,
    PropertyChangeReason reason) {
  opacity_ = opacity;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetVisibilityFromAnimation(
    bool visibility,
    PropertyChangeReason reason) {
  visibility_ = visibility;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetBrightnessFromAnimation(
    float brightness,
    PropertyChangeReason reason) {
  brightness_ = brightness;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetGrayscaleFromAnimation(
    float grayscale,
    PropertyChangeReason reason) {
  grayscale_ = grayscale;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetColorFromAnimation(
    SkColor color,
    PropertyChangeReason reason) {
  color_ = color;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetClipRectFromAnimation(
    const gfx::Rect& clip_rect,
    PropertyChangeReason reason) {
  clip_rect_ = clip_rect;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::SetRoundedCornersFromAnimation(
    const gfx::RoundedCornersF& rounded_corners,
    PropertyChangeReason reason) {
  rounded_corners_ = rounded_corners;
  last_property_change_reason_ = reason;
  last_property_change_reason_is_set_ = true;
}

void TestLayerAnimationDelegate::ScheduleDrawForAnimation() {
}

const gfx::Rect& TestLayerAnimationDelegate::GetBoundsForAnimation() const {
  return bounds_;
}

gfx::Transform TestLayerAnimationDelegate::GetTransformForAnimation() const {
  return transform_;
}

float TestLayerAnimationDelegate::GetOpacityForAnimation() const {
  return opacity_;
}

bool TestLayerAnimationDelegate::GetVisibilityForAnimation() const {
  return visibility_;
}

float TestLayerAnimationDelegate::GetBrightnessForAnimation() const {
  return brightness_;
}

float TestLayerAnimationDelegate::GetGrayscaleForAnimation() const {
  return grayscale_;
}

SkColor TestLayerAnimationDelegate::GetColorForAnimation() const {
  return color_;
}

gfx::Rect TestLayerAnimationDelegate::GetClipRectForAnimation() const {
  return clip_rect_;
}

gfx::RoundedCornersF TestLayerAnimationDelegate::GetRoundedCornersForAnimation()
    const {
  return rounded_corners_;
}

float TestLayerAnimationDelegate::GetDeviceScaleFactor() const {
  return 1.0f;
}

LayerAnimatorCollection*
TestLayerAnimationDelegate::GetLayerAnimatorCollection() {
  return nullptr;
}

ui::Layer* TestLayerAnimationDelegate::GetLayer() {
  return nullptr;
}

cc::Layer* TestLayerAnimationDelegate::GetCcLayer() const {
  return cc_layer_.get();
}

LayerThreadedAnimationDelegate*
TestLayerAnimationDelegate::GetThreadedAnimationDelegate() {
  return &threaded_delegate_;
}

int TestLayerAnimationDelegate::GetFrameNumber() const {
  return frame_number_;
}

float TestLayerAnimationDelegate::GetRefreshRate() const {
  return 60.0;
}


void TestLayerAnimationDelegate::CreateCcLayer() {
  cc_layer_ = cc::Layer::Create();
}

void TestLayerThreadedAnimationDelegate::AddThreadedAnimation(
    std::unique_ptr<cc::KeyframeModel> keyframe_model) {}

void TestLayerThreadedAnimationDelegate::RemoveThreadedAnimation(
    int keyframe_model_id) {}

}  // namespace ui
