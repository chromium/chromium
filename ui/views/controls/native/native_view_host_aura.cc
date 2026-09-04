// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_aura_with_clip_window.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views {

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host) : host_(host) {}

NativeViewHostAura::~NativeViewHostAura() {
  if (host_->native_view()) {
    native_view_observation_.Reset();
    if (host_->layer_managed_by_views()) {
      host_->native_view()->SetLayerManagedByParent(true);
    } else {
      host_->native_view()->ClearProperty(views::kHostViewKey);
    }
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    if (owned_by_parent_) {
      host_->native_view()->set_owned_by_parent(*owned_by_parent_);
    }
    if (host_->native_view()->parent()) {
      host_->native_view()->parent()->RemoveChild(host_->native_view());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::AttachNativeView() {
  CHECK(host_->GetWidget()->GetNativeView());
  owned_by_parent_ = host_->native_view()->owned_by_parent();
  host_->native_view()->set_owned_by_parent(false);

  if (!host_->layer_managed_by_views()) {
    host_->native_view()->SetProperty(views::kHostViewKey,
                                      static_cast<View*>(host_));
  }
  Widget::ReparentNativeView(host_->native_view(),
                             host_->GetWidget()->GetNativeView());
  if (host_->layer_managed_by_views()) {
    host_->native_view()->SetLayerManagedByParent(false);
    // Native views in views should be above independent native views
    host_->GetWidget()->GetNativeView()->StackChildAtBottom(
        host_->native_view());
    if (host_->create_layer()) {
      host_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    } else {
      CHECK(host_->layer());
    }
    host_->layer()->Add(GetUILayer());
  }
  native_view_observation_.Observe(host_->native_view());
  original_transform_ = host_->native_view()->transform();
  original_transform_changed_ = false;
  UpdateLayerClip();
  if (host_->layer_managed_by_views() && host_->GetWidget()) {
    host_->GetWidget()->ReorderNativeViews();
  }
}

void NativeViewHostAura::SetParentAccessible(
    gfx::NativeViewAccessible accessible) {
  host_->native_view()->SetProperty(
      aura::client::kParentNativeViewAccessibleKey, accessible);
}

gfx::NativeViewAccessible NativeViewHostAura::GetParentAccessible() {
  return host_->native_view()->GetProperty(
      aura::client::kParentNativeViewAccessibleKey);
}

ui::Layer* NativeViewHostAura::GetUILayer() {
  if (host_->native_view()) {
    return host_->native_view()->layer();
  }

  return nullptr;
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  CHECK(owned_by_parent_);
  host_->native_view()->set_owned_by_parent(*owned_by_parent_);
  owned_by_parent_.reset();
  if (host_->native_view()->parent()) {
    Widget::ReparentNativeView(host_->native_view(), nullptr);
  }

  if (!destroyed) {
    native_view_observation_.Reset();
    if (host_->layer_managed_by_views()) {
      host_->native_view()->SetLayerManagedByParent(true);
      if (host_->create_layer()) {
        host_->DestroyLayer();
      }
    } else {
      host_->native_view()->ClearProperty(views::kHostViewKey);
    }
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    if (original_transform_changed_) {
      host_->native_view()->SetTransform(original_transform_);
    }
    host_->native_view()->Hide();
  }
  if (!host_->native_view()->is_destroying()) {
    host_->native_view()->SetEventTargeter(nullptr);
  }
}

void NativeViewHostAura::AddedToWidget() {
  if (!host_->native_view()) {
    return;
  }
  if (host_->GetWidget()->GetNativeView()) {
    Widget::ReparentNativeView(host_->native_view(),
                               host_->GetWidget()->GetNativeView());
    if (host_->layer_managed_by_views()) {
      // Native views in views should be above independent native views, which
      // should be above views.
      host_->GetWidget()->GetNativeView()->StackChildAtBottom(
          host_->native_view());
    }
  }
  if (host_->IsDrawn()) {
    host_->native_view()->Show();
  } else {
    host_->native_view()->Hide();
  }
  host_->InvalidateLayout();
  UpdateLayerClip();
}

void NativeViewHostAura::RemovedFromWidget() {
  if (!host_->native_view()) {
    return;
  }

  host_->native_view()->Hide();
  if (host_->native_view()->parent()) {
    host_->native_view()->parent()->RemoveChild(host_->native_view());
  }
  if (!host_->native_view()->is_destroying()) {
    host_->native_view()->SetEventTargeter(nullptr);
  }
}

bool NativeViewHostAura::SetNativeViewCornerRadii(
    const gfx::RoundedCornersF& corner_radii) {
  corner_radii_ = corner_radii;
  ApplyRoundedCorners();
  return true;
}

gfx::RoundedCornersF NativeViewHostAura::GetNativeViewCornerRadii() const {
  return corner_radii_;
}

gfx::Rect NativeViewHostAura::GetNativeViewClipRect() const {
  ui::Layer* layer = const_cast<NativeViewHostAura*>(this)->GetUILayer();
  return layer ? layer->clip_rect() : gfx::Rect();
}

void NativeViewHostAura::SetHitTestTopInset(int top_inset) {
  if (host_->layer_managed_by_views()) {
    // Event targeting works correctly when the content layer is managed by
    // view.
    return;
  }

  if (top_inset_ == top_inset) {
    return;
  }
  top_inset_ = top_inset;
  UpdateLayerClip();
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  clip_rect_ = gfx::Rect(x, y, w, h);
}

int NativeViewHostAura::GetHitTestTopInset() const {
  return top_inset_;
}

bool NativeViewHostAura::HasInstalledClip() {
  return clip_rect_.has_value();
}

void NativeViewHostAura::UninstallClip() {
  clip_rect_.reset();
}

bool NativeViewHostAura::SetNativeViewClipRect(const gfx::Rect& clip_rect) {
  std::optional<gfx::Rect> new_clip =
      clip_rect.IsEmpty() ? std::nullopt : std::make_optional(clip_rect);
  if (external_clip_rect_ == new_clip) {
    return false;
  }
  external_clip_rect_ = new_clip;
  UpdateLayerClip();
  return true;
}

void NativeViewHostAura::ShowWidget(int x,
                                    int y,
                                    int w,
                                    int h,
                                    int native_w,
                                    int native_h) {
  if (host_->fast_resize()) {
    native_w = host_->native_view()->bounds().width();
    native_h = host_->native_view()->bounds().height();
    InstallClip(0, 0, w, h);
  } else {
    gfx::Transform transform = original_transform_;
    if (w > 0 && h > 0 && native_w > 0 && native_h > 0) {
      transform.Scale(static_cast<SkScalar>(w) / native_w,
                      static_cast<SkScalar>(h) / native_h);
    }
    // Only set the transform when it is actually different.
    if (transform != host_->native_view()->transform()) {
      host_->native_view()->SetTransform(transform);
      original_transform_changed_ = true;
    }
  }
  host_->native_view()->SetBounds({x, y, native_w, native_h});
  UpdateLayerClip();
  host_->native_view()->Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
}

void NativeViewHostAura::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client) {
    client->FocusWindow(window);
  }
}

gfx::NativeView NativeViewHostAura::GetNativeViewContainer() const {
  return host_->native_view();
}

gfx::NativeViewAccessible NativeViewHostAura::GetNativeViewAccessible() {
  return nullptr;
}

ui::Cursor NativeViewHostAura::GetCursor(int x, int y) {
  if (host_->native_view()) {
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  }
  return ui::Cursor();
}

void NativeViewHostAura::SetVisible(bool visible) {
  if (!visible) {
    host_->native_view()->Hide();
  } else {
    host_->native_view()->Show();
  }
}

void NativeViewHostAura::OnWindowDestroying(aura::Window* window) {
  DCHECK(window == host_->native_view());
}

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  native_view_observation_.Reset();
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  if (host->layer_managed_by_views()) {
    return new NativeViewHostAura(host);
  }
  return new NativeViewHostAuraWithClipWindow(host);
}

void NativeViewHostAura::ApplyRoundedCorners() {
  ui::Layer* layer = GetUILayer();
  if (!layer) {
    return;
  }

  gfx::RoundedCornersF radii = corner_radii_;
  if (base::i18n::IsRTL()) {
    radii = gfx::RoundedCornersF(radii.upper_right(), radii.upper_left(),
                                 radii.lower_left(), radii.lower_right());
  }

  gfx::Rect actual_clip = GetActualClipRect();
  if (!actual_clip.IsEmpty()) {
    const gfx::Size size = layer->size();
    if (actual_clip.x() > 0 || actual_clip.y() > 0) {
      radii.set_upper_left(0);
    }
    if (actual_clip.right() < size.width() || actual_clip.y() > 0) {
      radii.set_upper_right(0);
    }
    if (actual_clip.right() < size.width() ||
        actual_clip.bottom() < size.height()) {
      radii.set_lower_right(0);
    }
    if (actual_clip.x() > 0 || actual_clip.bottom() < size.height()) {
      radii.set_lower_left(0);
    }
  }

  if (layer->rounded_corner_radii() != radii) {
    layer->SetRoundedCornerRadius(radii);
    layer->SetIsFastRoundedCorner(true);
  }
}

void NativeViewHostAura::UpdateLayerClip() {
  ui::Layer* layer = GetUILayer();
  if (layer) {
    layer->SetClipRect(GetActualClipRect());
  }
  ApplyRoundedCorners();
}

gfx::Rect NativeViewHostAura::GetActualClipRect() const {
  gfx::Rect clip;
  if (clip_rect_) {
    clip = *clip_rect_;
  }

  // Intersect with external clip if present.
  if (external_clip_rect_) {
    if (clip.IsEmpty()) {
      clip = *external_clip_rect_;
    } else {
      clip.Intersect(*external_clip_rect_);
    }
  }

  if (top_inset_ > 0 && host_->native_view()) {
    double scale_y = 1.0;
    if (host_->native_view()->bounds().height() > 0 && host_->height() > 0) {
      scale_y = static_cast<double>(host_->height()) /
                host_->native_view()->bounds().height();
    }
    int native_top_inset = base::ClampRound(top_inset_ / scale_y);
    gfx::Rect allowed_rect(0, native_top_inset,
                           host_->native_view()->bounds().width(),
                           std::max(0, host_->native_view()->bounds().height() -
                                           native_top_inset));
    if (clip.IsEmpty()) {
      clip = allowed_rect;
    } else {
      clip.Intersect(allowed_rect);
    }
  }
  return clip;
}

}  // namespace views
