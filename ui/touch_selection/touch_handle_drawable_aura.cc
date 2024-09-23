// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle_drawable_aura.h"

#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/touch_selection//vector_icons/vector_icons.h"

namespace ui {
namespace {

// Vertical offset to apply from the bottom of the selection/text baseline to
// the top of the handle image. Only applied when touch text editing redesign is
// disabled.
constexpr int kSelectionHandleVerticalOffset = 2;

// Padding to apply horizontally around and vertically below the handle image,
// so that touch events near the handle image are targeted to the handle. Only
// applied when touch text editing redesign is enabled.
constexpr int kSelectionHandlePadding = 6;

// Max opacity of the selection handle image.
constexpr float kSelectionHandleMaxOpacity = 0.8f;

// Epsilon value used to compare float values to zero.
constexpr float kEpsilon = 1e-8f;

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
      NOTREACHED_IN_MIGRATION() << "Invalid touch handle bound type.";
      return nullptr;
  };
  return &ResourceBundle::GetSharedInstance().GetImageNamed(resource_id);
}

// Returns the appropriate handle vector icon based on the handle orientation.
ImageModel GetHandleVectorIcon(TouchHandleOrientation orientation) {
  const gfx::VectorIcon* icon = nullptr;
  switch (orientation) {
    case TouchHandleOrientation::LEFT:
      icon = &kTextSelectionHandleLeftIcon;
      break;
    case TouchHandleOrientation::CENTER:
      icon = &kTextSelectionHandleCenterIcon;
      break;
    case TouchHandleOrientation::RIGHT:
      icon = &kTextSelectionHandleRightIcon;
      break;
    case TouchHandleOrientation::UNDEFINED:
      NOTREACHED() << "Invalid touch handle bound type.";
  }
  return ImageModel::FromVectorIcon(*icon,
                                    /*color_id=*/kColorSysPrimary);
}

bool IsNearlyZero(float value) {
  return std::abs(value) < kEpsilon;
}

}  // namespace

TouchHandleDrawableAura::TouchHandleDrawableAura(aura::Window* parent)
    : window_(std::make_unique<aura::Window>(/*delegate=*/nullptr)),
      enabled_(false),
      alpha_(0),
      orientation_(TouchHandleOrientation::UNDEFINED) {
  window_->SetTransparent(true);
  window_->Init(LAYER_TEXTURED);
  window_->set_owned_by_parent(false);
  window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  window_->layer()->set_delegate(this);
  parent->AddChild(window_.get());

  theme_observation_.Observe(NativeTheme::GetInstanceForNativeUi());
}

TouchHandleDrawableAura::~TouchHandleDrawableAura() = default;

void TouchHandleDrawableAura::UpdateWindowBounds() {
  gfx::Rect window_bounds(gfx::ToRoundedPoint(targetable_origin_),
                          handle_image_.Size());
  // Offset the window bounds to account for space between the origin of the
  // targetable area and the handle image.
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    window_bounds.Offset(kSelectionHandlePadding, 0);
  } else {
    window_bounds.Offset(0, kSelectionHandleVerticalOffset);
  }
  window_->SetBounds(window_bounds);
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

  handle_image_ = ::features::IsTouchTextEditingRedesignEnabled()
                      ? GetHandleVectorIcon(orientation)
                      : ImageModel::FromImage(*GetHandleImage(orientation));
  UpdateWindowBounds();
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));
}

void TouchHandleDrawableAura::SetOrigin(const gfx::PointF& position) {
  targetable_origin_ = position;
  UpdateWindowBounds();
}

void TouchHandleDrawableAura::SetAlpha(float alpha) {
  if (alpha == alpha_)
    return;

  alpha_ = alpha;
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    window_->layer()->SetOpacity(alpha_ * kSelectionHandleMaxOpacity);
  } else {
    window_->layer()->SetOpacity(alpha_);
  }

  if (IsVisible())
    window_->Show();
  else
    window_->Hide();
}

gfx::RectF TouchHandleDrawableAura::GetVisibleBounds() const {
  // These bounds are used to determine the area that can be used for targeting
  // the handle, so we include the transparent padding added around the handle
  // image even though it technically isn't visible.
  gfx::RectF targetable_bounds(window_->bounds());
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    targetable_bounds.Outset(gfx::OutsetsF::TLBR(0, kSelectionHandlePadding,
                                                 kSelectionHandlePadding,
                                                 kSelectionHandlePadding));
  }
  return targetable_bounds;
}

float TouchHandleDrawableAura::GetDrawableHorizontalPaddingRatio() const {
  if (!::features::IsTouchTextEditingRedesignEnabled()) {
    return 0;
  }
  // The ratio returned by this function is used to position the touch handle
  // targetable area relative to the focal point (e.g. bottom of text caret).
  // So, even though padding is applied on both the left and right of the handle
  // image, we compute the ratio based on the padding on only one side.
  return kSelectionHandlePadding /
         (window_->bounds().width() + 2.0f * kSelectionHandlePadding);
}

void TouchHandleDrawableAura::OnPaintLayer(const PaintContext& context) {
  PaintRecorder recorder(context, window_->bounds().size());
  if (!handle_image_.IsEmpty()) {
    recorder.canvas()->DrawImageInt(
        handle_image_.Rasterize(ColorProviderManager::Get().GetColorProviderFor(
            NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
                nullptr))),
        0, 0);
  }
}

void TouchHandleDrawableAura::OnNativeThemeUpdated(
    NativeTheme* observed_theme) {
  if (!::features::IsTouchTextEditingRedesignEnabled()) {
    return;
  }
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));
}

}  // namespace ui
