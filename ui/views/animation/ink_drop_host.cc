// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host.h"

#include <utility>

#include "base/check_is_test.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
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
#include "ui/views/animation/pulsing_ink_drop_mask.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

namespace views {

// static
constexpr gfx::Size InkDropHost::kDefaultSquareInkDropSize;

InkDropHost::InkDropHostEventHandlerDelegate::InkDropHostEventHandlerDelegate(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host) {}

bool InkDropHost::InkDropHostEventHandlerDelegate::HasInkDrop() const {
  return ink_drop_host_->HasInkDrop();
}

InkDrop* InkDropHost::InkDropHostEventHandlerDelegate::GetInkDrop() {
  return ink_drop_host_->GetInkDrop();
}

bool InkDropHost::InkDropHostEventHandlerDelegate::SupportsGestureEvents()
    const {
  return ink_drop_host_->ink_drop_mode_ == InkDropMode::ON;
}

InkDropHost::ViewLayerTransformObserver::ViewLayerTransformObserver(
    InkDropHost* ink_drop_host,
    View* host_view)
    : ink_drop_host_(ink_drop_host) {
  observation_.Observe(host_view);
}

InkDropHost::ViewLayerTransformObserver::~ViewLayerTransformObserver() =
    default;

void InkDropHost::ViewLayerTransformObserver::OnViewLayerTransformed(
    View* observed_view) {
  // Notify the ink drop that we have transformed so it can adapt
  // accordingly.
  if (ink_drop_host_->HasInkDrop()) {
    ink_drop_host_->GetInkDrop()->HostTransformChanged(
        observed_view->GetTransform());
  }
}

InkDropHost::InkDropHost(View* view)
    : host_view_(view),
      host_view_transform_observer_(this, view),
      ink_drop_event_handler_delegate_(this),
      ink_drop_event_handler_(view, &ink_drop_event_handler_delegate_) {}

InkDropHost::~InkDropHost() {
  // Destroy ink_drop_ early as it holds references to this
  // and should not call methods on a partially-destructed
  // InkDropHost as part of its destruction.
  ink_drop_.reset();
}

std::unique_ptr<InkDrop> InkDropHost::CreateInkDrop() {
  if (create_ink_drop_callback_) {
    return create_ink_drop_callback_.Run();
  }
  return InkDrop::CreateInkDropForFloodFillRipple(this);
}

void InkDropHost::SetCreateInkDropCallback(
    base::RepeatingCallback<std::unique_ptr<InkDrop>()> callback) {
  create_ink_drop_callback_ = std::move(callback);
}

std::unique_ptr<InkDropRipple> InkDropHost::CreateInkDropRipple() const {
  if (create_ink_drop_ripple_callback_) {
    return create_ink_drop_ripple_callback_.Run();
  }
  return std::make_unique<views::FloodFillInkDropRipple>(
      InkDrop::Get(host_view_), host_view_->size(),
      GetInkDropCenterBasedOnLastEvent(), GetBaseColor(), GetVisibleOpacity());
}

void InkDropHost::SetCreateRippleCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropRipple>()> callback) {
  create_ink_drop_ripple_callback_ = std::move(callback);
}

gfx::Point InkDropHost::GetInkDropCenterBasedOnLastEvent() const {
  return GetEventHandler()->GetLastRippleTriggeringEvent()
             ? GetEventHandler()->GetLastRippleTriggeringEvent()->location()
             : host_view_->GetMirroredRect(host_view_->GetContentsBounds())
                   .CenterPoint();
}

std::unique_ptr<InkDropHighlight> InkDropHost::CreateInkDropHighlight() const {
  if (create_ink_drop_highlight_callback_) {
    return create_ink_drop_highlight_callback_.Run();
  }

  auto highlight = std::make_unique<views::InkDropHighlight>(
      host_view_->size(), 0,
      gfx::RectF(host_view_->GetMirroredRect(host_view_->GetLocalBounds()))
          .CenterPoint(),
      GetBaseColor());
  // TODO(pbos): Once |ink_drop_highlight_opacity_| is either always set or
  // callers are using the default InkDropHighlight value then make this a
  // constructor argument to InkDropHighlight.
  if (ink_drop_highlight_opacity_) {
    highlight->set_visible_opacity(*ink_drop_highlight_opacity_);
  }

  return highlight;
}

void InkDropHost::SetCreateHighlightCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropHighlight>()> callback) {
  create_ink_drop_highlight_callback_ = std::move(callback);
}

std::unique_ptr<views::InkDropMask> InkDropHost::CreateInkDropMask() const {
  // Attention mask takes precedence.
  if (in_attention_state_) {
    return std::make_unique<views::PulsingInkDropMask>(host_view_);
  }

  if (create_ink_drop_mask_callback_) {
    return create_ink_drop_mask_callback_.Run();
  }
  return std::make_unique<views::PathInkDropMask>(host_view_->size(),
                                                  GetHighlightPath(host_view_));
}

void InkDropHost::SetCreateMaskCallback(
    base::RepeatingCallback<std::unique_ptr<InkDropMask>()> callback) {
  create_ink_drop_mask_callback_ = std::move(callback);
}

void InkDropHost::ToggleAttentionState(bool attention_on) {
  in_attention_state_ = attention_on;

  // Calling HostSizeChanged() will force the new mask and color to be fetched.
  // TODO(collinbaker): Consider adding explicit way to recreate mask instead
  // of relying on HostSizeChanged() to do so.
  GetInkDrop()->HostSizeChanged(host_view_->size());
}

SkColor InkDropHost::GetBaseColor() const {
  // TODO(crbug.com/359904341): provide a fallback color provider for tests
  // that don't care about colors.
  if (!host_view_->GetWidget()) {
    CHECK_IS_TEST();
    return gfx::kPlaceholderColor;
  }

  // Attention color takes precedence.
  if (in_attention_state_) {
    ui::ColorProvider* const color_provider = host_view_->GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(ui::kColorButtonFeatureAttentionHighlight);
  }

  if (absl::holds_alternative<ui::ColorId>(ink_drop_base_color_)) {
    ui::ColorProvider* color_provider = host_view_->GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(
        absl::get<ui::ColorId>(ink_drop_base_color_));
  }

  if (absl::holds_alternative<SkColor>(ink_drop_base_color_)) {
    return absl::get<SkColor>(ink_drop_base_color_);
  }

  // The callback may need access to the color provider, which is only available
  // after the view is added to a widget.
  if (host_view_->GetWidget()) {
    return absl::get<base::RepeatingCallback<SkColor()>>(ink_drop_base_color_)
        .Run();
  }

  return gfx::kPlaceholderColor;
}

void InkDropHost::SetBaseColor(SkColor color) {
  ink_drop_base_color_ = color;
}

void InkDropHost::SetBaseColorId(ui::ColorId color_id) {
  ink_drop_base_color_ = color_id;
}

void InkDropHost::SetBaseColorCallback(
    base::RepeatingCallback<SkColor()> callback) {
  CHECK(callback);
  ink_drop_base_color_ = std::move(callback);
}

void InkDropHost::SetMode(InkDropMode ink_drop_mode) {
  ink_drop_mode_ = ink_drop_mode;
  ink_drop_.reset();
}

InkDropHost::InkDropMode InkDropHost::GetMode() const {
  return ink_drop_mode_;
}

void InkDropHost::SetLayerRegion(LayerRegion region) {
  layer_region_ = region;
  ink_drop_.reset();
}

LayerRegion InkDropHost::GetLayerRegion() const {
  return layer_region_;
}

void InkDropHost::SetVisibleOpacity(float visible_opacity) {
  if (visible_opacity == ink_drop_visible_opacity_) {
    return;
  }
  ink_drop_visible_opacity_ = visible_opacity;
}

float InkDropHost::GetVisibleOpacity() const {
  return ink_drop_visible_opacity_;
}

void InkDropHost::SetHighlightOpacity(std::optional<float> opacity) {
  if (opacity == ink_drop_highlight_opacity_) {
    return;
  }
  ink_drop_highlight_opacity_ = opacity;
}

void InkDropHost::SetSmallCornerRadius(int small_radius) {
  if (small_radius == ink_drop_small_corner_radius_) {
    return;
  }
  ink_drop_small_corner_radius_ = small_radius;
}

int InkDropHost::GetSmallCornerRadius() const {
  return ink_drop_small_corner_radius_;
}

void InkDropHost::SetLargeCornerRadius(int large_radius) {
  if (large_radius == ink_drop_large_corner_radius_) {
    return;
  }
  ink_drop_large_corner_radius_ = large_radius;
}

int InkDropHost::GetLargeCornerRadius() const {
  return ink_drop_large_corner_radius_;
}

void InkDropHost::AnimateToState(InkDropState state,
                                 const ui::LocatedEvent* event) {
  GetEventHandler()->AnimateToState(state, event);
}

bool InkDropHost::HasInkDrop() const {
  return !!ink_drop_;
}

InkDrop* InkDropHost::GetInkDrop() {
  if (!ink_drop_) {
    if (ink_drop_mode_ == InkDropMode::OFF) {
      ink_drop_ = std::make_unique<InkDropStub>();
    } else {
      ink_drop_ = CreateInkDrop();
    }
  }
  return ink_drop_.get();
}

bool InkDropHost::GetHighlighted() const {
  return ink_drop_ && ink_drop_->IsHighlightFadingInOrVisible();
}

base::CallbackListSubscription InkDropHost::AddHighlightedChangedCallback(
    base::RepeatingClosure callback) {
  return highlighted_changed_callbacks_.Add(std::move(callback));
}

void InkDropHost::OnInkDropHighlightedChanged() {
  highlighted_changed_callbacks_.Notify();
}

void InkDropHost::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  // If a clip is provided, use that as it is more performant than a mask.
  // If `host_view_` is in attention state e.g. has an IPH bubble attached
  // also install the attention mask.
  if (!AddInkDropClip(ink_drop_layer) || in_attention_state_) {
    InstallInkDropMask(ink_drop_layer);
  }
  host_view_->AddLayerToRegion(ink_drop_layer, layer_region_);
}

void InkDropHost::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  host_view_->RemoveLayerFromRegions(ink_drop_layer);

  // Remove clipping.
  ink_drop_layer->SetClipRect(gfx::Rect());
  ink_drop_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(0.f));

  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
}

// static
gfx::Size InkDropHost::GetLargeSize(gfx::Size small_size) {
  constexpr float kLargeInkDropScale = 1.333f;
  return gfx::ScaleToCeiledSize(small_size, kLargeInkDropScale);
}

std::unique_ptr<InkDropRipple> InkDropHost::CreateSquareRipple(
    const gfx::Point& center_point,
    const gfx::Size& size) const {
  auto ripple = std::make_unique<SquareInkDropRipple>(
      InkDrop::Get(host_view_), GetLargeSize(size),
      ink_drop_large_corner_radius_, size, ink_drop_small_corner_radius_,
      center_point, GetBaseColor(), GetVisibleOpacity());
  return ripple;
}

const InkDropEventHandler* InkDropHost::GetEventHandler() const {
  return &ink_drop_event_handler_;
}

InkDropEventHandler* InkDropHost::GetEventHandler() {
  return const_cast<InkDropEventHandler*>(
      const_cast<const InkDropHost*>(this)->GetEventHandler());
}

bool InkDropHost::AddInkDropClip(ui::Layer* ink_drop_layer) {
  std::optional<gfx::RRectF> clipping_data =
      HighlightPathGenerator::GetRoundRectForView(host_view_);
  if (!clipping_data) {
    return false;
  }

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

void InkDropHost::InstallInkDropMask(ui::Layer* ink_drop_layer) {
  ink_drop_mask_ = CreateInkDropMask();
  DCHECK(ink_drop_mask_);
  ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
}

}  // namespace views
