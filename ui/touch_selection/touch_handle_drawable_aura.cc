// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle_drawable_aura.h"

#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura_extra/image_window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/resources/grit/ui_resources.h"

namespace ui {
namespace {

// The distance by which a handle image is offset from the focal point (i.e.
// text baseline) downwards.
const int kSelectionHandleVerticalVisualOffset = 2;

// The padding around the selection handle image can be used to extend the
// handle window so that touch events near the selection handle image are
// targeted to the selection handle window.
const int kSelectionHandlePadding = 0;

// Epsilon value used to compare float values to zero.
const float kEpsilon = 1e-8f;

// Returns the appropriate handle image based on the handle orientation.
gfx::Image* GetHandleImage(TouchHandleOrientation orientation) {
  int resource_id = 0;
  switch (orientation) {
    case TouchHandleOrientation::LEFT:
      resource_id = IDR_TEXT_SELECTION_HANDLE_LEFT;
      break;
    case TouchHandleOrientation::CENTER:
      resource_id = IDR_TEXT_SELECTION_HANDLE_CENTER;
      break;
    case TouchHandleOrientation::RIGHT:
      resource_id = IDR_TEXT_SELECTION_HANDLE_RIGHT;
      break;
    case TouchHandleOrientation::UNDEFINED:
      NOTREACHED() << "Invalid touch handle bound type.";
      return nullptr;
  };
  return &ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
}

bool IsNearlyZero(float value) {
  return std::abs(value) < kEpsilon;
}

}  // namespace

TouchHandleDrawableAura::TouchHandleDrawableAura(aura::Window* parent)
    : window_delegate_(new aura_extra::ImageWindowDelegate),
      window_(new aura::Window(window_delegate_)),
      enabled_(false),
      alpha_(0),
      orientation_(TouchHandleOrientation::UNDEFINED) {
  window_delegate_->set_image_offset(gfx::Vector2d(kSelectionHandlePadding,
                                                   kSelectionHandlePadding));
  window_delegate_->set_background_color(SK_ColorTRANSPARENT);
  window_->SetTransparent(true);
  window_->Init(LAYER_TEXTURED);
  window_->set_owned_by_parent(false);
  window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  parent->AddChild(window_.get());
}

TouchHandleDrawableAura::~TouchHandleDrawableAura() {
}

void TouchHandleDrawableAura::UpdateBounds() {
  gfx::RectF new_bounds = relative_bounds_;
  new_bounds.Offset(origin_position_.x(), origin_position_.y());
  window_->SetBounds(gfx::ToEnclosingRect(new_bounds));
}

bool TouchHandleDrawableAura::IsVisible() const {
  return enabled_ && !IsNearlyZero(alpha_);
}

void TouchHandleDrawableAura::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  if (IsVisible())
    window_->Show();
  else
    window_->Hide();
}

void TouchHandleDrawableAura::SetOrientation(TouchHandleOrientation orientation,
                                             bool mirror_vertical,
                                             bool mirror_horizontal) {
  // TODO(AviD): Implement adaptive handle orientation logic for Aura
  DCHECK(!mirror_vertical);
  DCHECK(!mirror_horizontal);

  if (orientation_ == orientation)
    return;
  orientation_ = orientation;
  gfx::Image* image = GetHandleImage(orientation);
  window_delegate_->SetImage(*image);

  // Calculate the relative bounds.
  gfx::Size image_size = image->Size();
  int window_width = image_size.width() + 2 * kSelectionHandlePadding;
  int window_height = image_size.height() + 2 * kSelectionHandlePadding;
  relative_bounds_ =
      gfx::RectF(-kSelectionHandlePadding,
                 kSelectionHandleVerticalVisualOffset - kSelectionHandlePadding,
                 window_width, window_height);
  gfx::Rect paint_bounds(relative_bounds_.x(), relative_bounds_.y(),
                         relative_bounds_.width(), relative_bounds_.height());
  window_->SchedulePaintInRect(paint_bounds);
  UpdateBounds();
}

void TouchHandleDrawableAura::SetOrigin(const gfx::PointF& position) {
  origin_position_ = position;
  UpdateBounds();
}

void TouchHandleDrawableAura::SetAlpha(float alpha) {
  if (alpha == alpha_)
    return;

  alpha_ = alpha;
  window_->layer()->SetOpacity(alpha_);
  if (IsVisible())
    window_->Show();
  else
    window_->Hide();
}

gfx::RectF TouchHandleDrawableAura::GetVisibleBounds() const {
  gfx::RectF bounds(window_->bounds());
  bounds.Inset(kSelectionHandlePadding,
               kSelectionHandlePadding + kSelectionHandleVerticalVisualOffset,
               kSelectionHandlePadding,
               kSelectionHandlePadding);
  return bounds;
}

float TouchHandleDrawableAura::GetDrawableHorizontalPaddingRatio() const {
  // Aura does not have any transparent padding for its handle drawable.
  return 0.0f;
}

}  // namespace ui
