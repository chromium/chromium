// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_stub.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

namespace views {

// static
constexpr gfx::Size InkDropHostView::kDefaultInkDropSize;

InkDropHostView::InkDropHostViewEventHandlerDelegate::
    InkDropHostViewEventHandlerDelegate(InkDropHostView* host_view)
    : host_view_(host_view) {}

bool InkDropHostView::InkDropHostViewEventHandlerDelegate::HasInkDrop() const {
  return host_view_->HasInkDrop();
}

InkDrop* InkDropHostView::InkDropHostViewEventHandlerDelegate::GetInkDrop() {
  return host_view_->GetInkDrop();
}

bool InkDropHostView::InkDropHostViewEventHandlerDelegate::
    SupportsGestureEvents() const {
  return host_view_->ink_drop_mode_ == InkDropMode::ON;
}

InkDropHostView::InkDropHostView()
    : ink_drop_event_handler_delegate_(this),
      ink_drop_event_handler_(this, &ink_drop_event_handler_delegate_) {}

InkDropHostView::~InkDropHostView() {
  // TODO(bruthig): Improve InkDropImpl to be safer about calling back to
  // potentially destroyed InkDropHosts and remove |destroying_|.
  destroying_ = true;
}

void InkDropHostView::SetAddInkDropLayerCallback(
    base::RepeatingCallback<void(ui::Layer*)> callback) {
  add_ink_drop_layer_callback_ = std::move(callback);
}

const base::RepeatingCallback<void(ui::Layer*)>&
InkDropHostView::GetAddInkDropLayerCallback() const {
  return add_ink_drop_layer_callback_;
}

void InkDropHostView::SetRemoveInkDropLayerCallback(
    base::RepeatingCallback<void(ui::Layer*)> callback) {
  remove_ink_drop_layer_callback_ = std::move(callback);
}

const base::RepeatingCallback<void(ui::Layer*)>&
InkDropHostView::GetRemoveInkDropLayerCallback() const {
  return remove_ink_drop_layer_callback_;
}

std::unique_ptr<InkDrop> InkDropHostView::CreateInkDrop() {
  if (create_ink_drop_callback_)
    return create_ink_drop_callback_.Run();
  return InkDrop::CreateInkDropForFloodFillRipple(this);
}

void InkDropHostView::SetCreateInkDropCallback(
    base::RepeatingCallback<std::unique_ptr<InkDrop>()> callback) {
  create_ink_drop_callback_ = std::move(callback);
}

const base::RepeatingCallback<std::unique_ptr<InkDrop>()>&
InkDropHostView::GetCreateInkDropCallback() const {
  return create_ink_drop_callback_;
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateInkDropRipple() const {
  if (create_ink_drop_ripple_callback_)
    return create_ink_drop_ripple_callback_.Run();
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
      GetInkDropBaseColor(), GetInkDropVisibleOpacity());
}

void InkDropHostView::SetCreateInkDropRippleCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropRipple>()> callback) {
  create_ink_drop_ripple_callback_ = std::move(callback);
}

const base::RepeatingCallback<std::unique_ptr<InkDropRipple>()>&
InkDropHostView::GetCreateInkDropRippleCallback() const {
  return create_ink_drop_ripple_callback_;
}

gfx::Point InkDropHostView::GetInkDropCenterBasedOnLastEvent() const {
  return GetEventHandler()->GetLastRippleTriggeringEvent()
             ? GetEventHandler()->GetLastRippleTriggeringEvent()->location()
             : GetMirroredRect(GetContentsBounds()).CenterPoint();
}

std::unique_ptr<InkDropHighlight> InkDropHostView::CreateInkDropHighlight()
    const {
  if (create_ink_drop_highlight_callback_)
    return create_ink_drop_highlight_callback_.Run();

  auto highlight = std::make_unique<views::InkDropHighlight>(
      size(), 0, gfx::RectF(GetMirroredRect(GetLocalBounds())).CenterPoint(),
      GetInkDropBaseColor());
  // TODO(pbos): Once |ink_drop_highlight_opacity_| is either always set or
  // callers are using the default InkDropHighlight value then make this a
  // constructor argument to InkDropHighlight.
  if (ink_drop_highlight_opacity_)
    highlight->set_visible_opacity(*ink_drop_highlight_opacity_);

  return highlight;
}

void InkDropHostView::SetCreateInkDropHighlightCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()> callback) {
  create_ink_drop_highlight_callback_ = std::move(callback);
}

const base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()>&
InkDropHostView::GetCreateInkDropHighlightCallback() const {
  return create_ink_drop_highlight_callback_;
}

std::unique_ptr<views::InkDropMask> InkDropHostView::CreateInkDropMask() const {
  if (create_ink_drop_mask_callback_)
    return create_ink_drop_mask_callback_.Run();
  return std::make_unique<views::PathInkDropMask>(size(),
                                                  GetHighlightPath(this));
}

void InkDropHostView::SetCreateInkDropMaskCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropMask>()> callback) {
  create_ink_drop_mask_callback_ = std::move(callback);
}

const base::RepeatingCallback<std::unique_ptr<InkDropMask>()>&
InkDropHostView::GetCreateInkDropMaskCallback() const {
  return create_ink_drop_mask_callback_;
}

SkColor InkDropHostView::GetInkDropBaseColor() const {
  if (ink_drop_base_color_callback_)
    return ink_drop_base_color_callback_.Run();
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void InkDropHostView::SetInkDropBaseColorCallback(
    base::RepeatingCallback<SkColor()> callback) {
  ink_drop_base_color_callback_ = std::move(callback);
}

const base::RepeatingCallback<SkColor()>&
InkDropHostView::GetInkDropBaseColorCallback() const {
  return ink_drop_base_color_callback_;
}

void InkDropHostView::SetInkDropMode(InkDropMode ink_drop_mode) {
  ink_drop_mode_ = ink_drop_mode;
  ink_drop_ = nullptr;
}

void InkDropHostView::SetInkDropVisibleOpacity(float visible_opacity) {
  if (visible_opacity == ink_drop_visible_opacity_)
    return;
  ink_drop_visible_opacity_ = visible_opacity;
  OnPropertyChanged(&ink_drop_visible_opacity_, kPropertyEffectsPaint);
}

float InkDropHostView::GetInkDropVisibleOpacity() const {
  return ink_drop_visible_opacity_;
}

void InkDropHostView::SetInkDropHighlightOpacity(
    base::Optional<float> opacity) {
  if (opacity == ink_drop_highlight_opacity_)
    return;
  ink_drop_highlight_opacity_ = opacity;
  OnPropertyChanged(&ink_drop_highlight_opacity_, kPropertyEffectsPaint);
}

base::Optional<float> InkDropHostView::GetInkDropHighlightOpacity() const {
  return ink_drop_highlight_opacity_;
}

void InkDropHostView::SetInkDropSmallCornerRadius(int small_radius) {
  if (small_radius == ink_drop_small_corner_radius_)
    return;
  ink_drop_small_corner_radius_ = small_radius;
  OnPropertyChanged(&ink_drop_small_corner_radius_, kPropertyEffectsLayout);
}

int InkDropHostView::GetInkDropSmallCornerRadius() const {
  return ink_drop_small_corner_radius_;
}

void InkDropHostView::SetInkDropLargeCornerRadius(int large_radius) {
  if (large_radius == ink_drop_large_corner_radius_)
    return;
  ink_drop_large_corner_radius_ = large_radius;
  OnPropertyChanged(&ink_drop_large_corner_radius_, kPropertyEffectsLayout);
}

int InkDropHostView::GetInkDropLargeCornerRadius() const {
  return ink_drop_large_corner_radius_;
}

void InkDropHostView::AnimateInkDrop(InkDropState state,
                                     const ui::LocatedEvent* event) {
  GetEventHandler()->AnimateInkDrop(state, event);
}

InkDrop* InkDropHostView::GetInkDrop() {
  if (!ink_drop_) {
    if (ink_drop_mode_ == InkDropMode::OFF)
      ink_drop_ = std::make_unique<InkDropStub>();
    else
      ink_drop_ = CreateInkDrop();
  }
  return ink_drop_.get();
}

bool InkDropHostView::GetHighlighted() const {
  return ink_drop_ && ink_drop_->IsHighlightFadingInOrVisible();
}

base::CallbackListSubscription InkDropHostView::AddHighlightedChangedCallback(
    PropertyChangedCallback callback) {
  // Since the highlight state is not directly represented by a member, use the
  // applicable member (|ink_drop_|) as the property key.  Note that this won't
  // suffice if a future InkDrop-related property is added.
  return AddPropertyChangedCallback(&ink_drop_, std::move(callback));
}

void InkDropHostView::OnInkDropHighlightedChanged() {
  // See comments in AddHighlightedChangedCallback() re: using |ink_drop_| as
  // the key.
  OnPropertyChanged(&ink_drop_, kPropertyEffectsNone);
}

void InkDropHostView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  if (add_ink_drop_layer_callback_) {
    add_ink_drop_layer_callback_.Run(ink_drop_layer);
    return;
  }

  // If a clip is provided, use that as it is more performant than a mask.
  if (!AddInkDropClip(ink_drop_layer))
    InstallInkDropMask(ink_drop_layer);
  AddLayerBeneathView(ink_drop_layer);
}

void InkDropHostView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  // No need to do anything when called during shutdown, and if a derived
  // class has set `remove_ink_drop_layer_callback_` then running that callback
  // is very likely to be a use-after-free.
  if (destroying_)
    return;

  if (remove_ink_drop_layer_callback_) {
    remove_ink_drop_layer_callback_.Run(ink_drop_layer);
    return;
  }

  RemoveLayerBeneathView(ink_drop_layer);

  // Remove clipping.
  ink_drop_layer->SetClipRect(gfx::Rect());
  ink_drop_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(0.f));

  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateInkDropForSquareRipple(
    const gfx::Point& center_point,
    const gfx::Size& size) const {
  auto ripple = std::make_unique<SquareInkDropRipple>(
      CalculateLargeInkDropSize(size), ink_drop_large_corner_radius_, size,
      ink_drop_small_corner_radius_, center_point, GetInkDropBaseColor(),
      GetInkDropVisibleOpacity());
  return ripple;
}

bool InkDropHostView::HasInkDrop() const {
  return !!ink_drop_;
}

// static
gfx::Size InkDropHostView::CalculateLargeInkDropSize(
    const gfx::Size& small_size) {
  // The scale factor to compute the large size of the default
  // SquareInkDropRipple.
  constexpr float kLargeInkDropScale = 1.333f;
  return gfx::ScaleToCeiledSize(gfx::Size(small_size), kLargeInkDropScale);
}

void InkDropHostView::OnLayerTransformed(const gfx::Transform& old_transform,
                                         ui::PropertyChangeReason reason) {
  View::OnLayerTransformed(old_transform, reason);

  // Notify the ink drop that we have transformed so it can adapt accordingly.
  if (HasInkDrop())
    GetInkDrop()->HostTransformChanged(GetTransform());
}

const InkDropEventHandler* InkDropHostView::GetEventHandler() const {
  if (ink_drop_event_handler_override_)
    return ink_drop_event_handler_override_;
  return &ink_drop_event_handler_;
}

InkDropEventHandler* InkDropHostView::GetEventHandler() {
  return const_cast<InkDropEventHandler*>(
      const_cast<const InkDropHostView*>(this)->GetEventHandler());
}

bool InkDropHostView::AddInkDropClip(ui::Layer* ink_drop_layer) {
  base::Optional<gfx::RRectF> clipping_data =
      HighlightPathGenerator::GetRoundRectForView(this);
  if (!clipping_data)
    return false;

  ink_drop_layer->SetClipRect(gfx::ToEnclosingRect(clipping_data->rect()));
  auto get_corner_radii =
      [&clipping_data](gfx::RRectF::Corner corner) -> float {
    return clipping_data.value().GetCornerRadii(corner).x();
  };
  gfx::RoundedCornersF rounded_corners;
  rounded_corners.set_upper_left(
      get_corner_radii(gfx::RRectF::Corner::kUpperLeft));
  rounded_corners.set_upper_right(
      get_corner_radii(gfx::RRectF::Corner::kUpperRight));
  rounded_corners.set_lower_right(
      get_corner_radii(gfx::RRectF::Corner::kLowerRight));
  rounded_corners.set_lower_left(
      get_corner_radii(gfx::RRectF::Corner::kLowerLeft));
  ink_drop_layer->SetRoundedCornerRadius(rounded_corners);
  ink_drop_layer->SetIsFastRoundedCorner(true);
  return true;
}

void InkDropHostView::InstallInkDropMask(ui::Layer* ink_drop_layer) {
  ink_drop_mask_ = CreateInkDropMask();
  DCHECK(ink_drop_mask_);
  ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
}

BEGIN_METADATA(InkDropHostView, View)
ADD_PROPERTY_METADATA(base::RepeatingCallback<void(ui::Layer*)>,
                      AddInkDropLayerCallback)
ADD_PROPERTY_METADATA(base::RepeatingCallback<void(ui::Layer*)>,
                      RemoveInkDropLayerCallback)
ADD_PROPERTY_METADATA(base::RepeatingCallback<std::unique_ptr<InkDrop>()>,
                      CreateInkDropCallback)
ADD_PROPERTY_METADATA(base::RepeatingCallback<std::unique_ptr<InkDropRipple>()>,
                      CreateInkDropRippleCallback)
ADD_PROPERTY_METADATA(
    base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()>,
    CreateInkDropHighlightCallback)
ADD_PROPERTY_METADATA(base::RepeatingCallback<std::unique_ptr<InkDropMask>()>,
                      CreateInkDropMaskCallback)
ADD_PROPERTY_METADATA(base::RepeatingCallback<SkColor()>,
                      InkDropBaseColorCallback)
ADD_READONLY_PROPERTY_METADATA(bool, Highlighted)
ADD_PROPERTY_METADATA(float, InkDropVisibleOpacity)
ADD_PROPERTY_METADATA(base::Optional<float>, InkDropHighlightOpacity)
ADD_PROPERTY_METADATA(int, InkDropLargeCornerRadius)
ADD_PROPERTY_METADATA(int, InkDropSmallCornerRadius)
END_METADATA

}  // namespace views
