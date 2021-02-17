// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

InstallableInkDropConfig GetPlaceholderInstallableInkDropConfig() {
  InstallableInkDropConfig config{0};
  config.base_color = gfx::kPlaceholderColor;
  config.ripple_opacity = 1.0f;
  config.highlight_opacity = 1.0f;
  return config;
}

}  // namespace

const base::Feature kInstallableInkDropFeature{
    "InstallableInkDrop", base::FEATURE_DISABLED_BY_DEFAULT};

InstallableInkDrop::InstallableInkDrop(View* view)
    : view_(view),
      config_(GetPlaceholderInstallableInkDropConfig()),
      layer_(std::make_unique<ui::Layer>()),
      event_handler_(view_, this),
      painter_(&config_, &visual_state_),
      animation_container_(base::MakeRefCounted<gfx::AnimationContainer>()),
      animator_(layer_->size(),
                &visual_state_,
                animation_container_.get(),
                base::BindRepeating(&InstallableInkDrop::SchedulePaint,
                                    base::Unretained(this))) {
  // Catch if |view_| is destroyed out from under us.
  if (DCHECK_IS_ON())
    view_->AddObserver(this);

  layer_->set_delegate(this);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetFillsBoundsCompletely(false);
  view_->AddLayerBeneathView(layer_.get());

  // AddLayerBeneathView() changes the location of |layer_| so this must be done
  // after.
  layer_->SetBounds(gfx::Rect(view_->size()) +
                    layer_->bounds().OffsetFromOrigin());
  layer_->SchedulePaint(gfx::Rect(layer_->size()));

  if (view_->GetWidget()) {
    // Using CompositorAnimationRunner keeps our animation updates in sync with
    // compositor frames and avoids jank.
    animation_container_->SetAnimationRunner(
        std::make_unique<CompositorAnimationRunner>(view_->GetWidget()));
  }
}

InstallableInkDrop::InstallableInkDrop(InkDropHostView* ink_drop_host_view)
    : InstallableInkDrop(static_cast<View*>(ink_drop_host_view)) {
  // To get all events, we must override InkDropHostView's event handler.
  ink_drop_host_view->set_ink_drop_event_handler_override(&event_handler_);
  ink_drop_host_view_ = ink_drop_host_view;

  // TODO(crbug.com/931964): When this is removed, classes relying on property
  // changed notifications from InkDropHostView for the highlighted state will
  // need to register here instead.
  RegisterHighlightedChangedCallback(
      base::BindRepeating(&InkDropHostView::OnInkDropHighlightedChanged,
                          base::Unretained(ink_drop_host_view_)));
}

InstallableInkDrop::~InstallableInkDrop() {
  view_->RemoveLayerBeneathView(layer_.get());
  if (ink_drop_host_view_)
    ink_drop_host_view_->set_ink_drop_event_handler_override(nullptr);
  if (DCHECK_IS_ON())
    view_->RemoveObserver(this);
}

void InstallableInkDrop::SetConfig(InstallableInkDropConfig config) {
  config_ = config;
  SchedulePaint();
}

base::CallbackListSubscription
InstallableInkDrop::RegisterHighlightedChangedCallback(
    base::RepeatingClosure callback) {
  return highlighted_changed_list_.Add(std::move(callback));
}

void InstallableInkDrop::HostSizeChanged(const gfx::Size& new_size) {
  layer_->SetBounds(gfx::Rect(new_size) + layer_->bounds().OffsetFromOrigin());
  layer_->SchedulePaint(gfx::Rect(layer_->size()));
  animator_.SetSize(layer_->size());
}

void InstallableInkDrop::HostTransformChanged(
    const gfx::Transform& new_transform) {}

InkDropState InstallableInkDrop::GetTargetInkDropState() const {
  return animator_.target_state();
}

void InstallableInkDrop::AnimateToState(InkDropState ink_drop_state) {
  const gfx::Point ripple_center =
      event_handler_.GetLastRippleTriggeringEvent()
          ? event_handler_.GetLastRippleTriggeringEvent()->location()
          : view_->GetMirroredRect(view_->GetLocalBounds()).CenterPoint();
  animator_.SetLastEventLocation(ripple_center);
  animator_.AnimateToState(ink_drop_state);
}

void InstallableInkDrop::SetHoverHighlightFadeDuration(
    base::TimeDelta duration) {
  NOTREACHED();
}

void InstallableInkDrop::UseDefaultHoverHighlightFadeDuration() {
  NOTREACHED();
}

void InstallableInkDrop::SnapToActivated() {
  // TODO(crbug.com/933384): do this without animation.
  animator_.AnimateToState(InkDropState::ACTIVATED);
}

void InstallableInkDrop::SnapToHidden() {
  // TODO(crbug.com/933384): do this without animation.
  animator_.AnimateToState(InkDropState::HIDDEN);
}

void InstallableInkDrop::SetHovered(bool is_hovered) {
  if (is_hovered_ == is_hovered)
    return;
  is_hovered_ = is_hovered;
  UpdateAnimatorHighlight();
}

void InstallableInkDrop::SetFocused(bool is_focused) {
  if (is_focused_ == is_focused)
    return;
  is_focused_ = is_focused;
  UpdateAnimatorHighlight();
}

bool InstallableInkDrop::IsHighlightFadingInOrVisible() const {
  return is_hovered_ || is_focused_;
}

void InstallableInkDrop::SetShowHighlightOnHover(bool show_highlight_on_hover) {
  NOTREACHED();
}

void InstallableInkDrop::SetShowHighlightOnFocus(bool show_highlight_on_focus) {
  NOTREACHED();
}

InkDrop* InstallableInkDrop::GetInkDrop() {
  return this;
}

bool InstallableInkDrop::HasInkDrop() const {
  return true;
}

bool InstallableInkDrop::SupportsGestureEvents() const {
  return true;
}

void InstallableInkDrop::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(view_, observed_view);
  NOTREACHED() << "|this| needs to outlive the view it's installed on";
}

void InstallableInkDrop::OnPaintLayer(const ui::PaintContext& context) {
  DCHECK_EQ(view_->size(), layer_->size());

  ui::PaintRecorder paint_recorder(context, layer_->size());
  gfx::Canvas* canvas = paint_recorder.canvas();
  canvas->ClipPath(GetHighlightPath(view_), true);

  painter_.Paint(canvas, view_->size());
}

void InstallableInkDrop::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void InstallableInkDrop::SchedulePaint() {
  layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

void InstallableInkDrop::UpdateAnimatorHighlight() {
  animator_.AnimateHighlight(is_hovered_ || is_focused_);
  highlighted_changed_list_.Notify();
}

}  // namespace views
