// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura_with_clip_window.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
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
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeViewHostAuraWithClipWindow::ClippingWindowDelegate
    : public aura::WindowDelegate {
 public:
  ClippingWindowDelegate() = default;
  ~ClippingWindowDelegate() override = default;

  void set_native_view(aura::Window* native_view) {
    native_view_ = native_view;
  }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  std::optional<gfx::Size> GetMaximumSize() const override {
    return std::nullopt;
  }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  ui::Cursor GetCursor(const gfx::Point& point) override {
    return ui::Cursor();
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCLIENT;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override {
    // Ask the hosted native view's delegate because directly calling
    // aura::Window::CanFocus() will call back into this when checking whether
    // parents can focus.
    return !native_view_ || !native_view_->delegate() ||
           native_view_->delegate()->CanFocus();
  }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(SkPath* mask) const override {}

 private:
  raw_ptr<aura::Window> native_view_ = nullptr;
};

NativeViewHostAuraWithClipWindow::NativeViewHostAuraWithClipWindow(
    NativeViewHost* host)
    : host_(host) {}

NativeViewHostAuraWithClipWindow::~NativeViewHostAuraWithClipWindow() {
  if (host_->native_view()) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    clipping_window_->ClearProperty(views::kHostViewKey);
    if (host_->native_view()->parent() == clipping_window_.get()) {
      clipping_window_->RemoveChild(host_->native_view());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAuraWithClipWindow, NativeViewHostWrapper implementation:
void NativeViewHostAuraWithClipWindow::AttachNativeView() {
  if (!clipping_window_) {
    CreateClippingWindow();
  }
  clipping_window_delegate_->set_native_view(host_->native_view());
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
                                    static_cast<View*>(host_));

  original_transform_ = host_->native_view()->transform();
  original_transform_changed_ = false;
  AddClippingWindow();
  ApplyRoundedCorners();
}

void NativeViewHostAuraWithClipWindow::SetParentAccessible(
    gfx::NativeViewAccessible accessible) {
  host_->native_view()->SetProperty(
      aura::client::kParentNativeViewAccessibleKey, accessible);
}

gfx::NativeViewAccessible
NativeViewHostAuraWithClipWindow::GetParentAccessible() {
  return host_->native_view()->GetProperty(
      aura::client::kParentNativeViewAccessibleKey);
}

ui::Layer* NativeViewHostAuraWithClipWindow::GetUILayer() {
  if (host_->native_view()) {
    return host_->native_view()->layer();
  }

  return nullptr;
}

void NativeViewHostAuraWithClipWindow::NativeViewDetaching(bool destroyed) {
  // This method causes a succession of window tree changes. ScopedPause ensures
  // that occlusion is recomputed at the end of the method instead of after each
  // change.
  std::optional<aura::WindowOcclusionTracker::ScopedPause> pause_occlusion;
  if (clipping_window_) {
    pause_occlusion.emplace();
  }

  clipping_window_delegate_->set_native_view(nullptr);
  RemoveClippingWindow();
  if (!destroyed) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    if (original_transform_changed_) {
      host_->native_view()->SetTransform(original_transform_);
    }
    host_->native_view()->Hide();
    if (host_->native_view()->parent()) {
      Widget::ReparentNativeView(host_->native_view(), nullptr);
    }
  }
}

void NativeViewHostAuraWithClipWindow::AddedToWidget() {
  if (!host_->native_view()) {
    return;
  }

  AddClippingWindow();
  if (host_->IsDrawn()) {
    host_->native_view()->Show();
  } else {
    host_->native_view()->Hide();
  }
  host_->InvalidateLayout();
}

void NativeViewHostAuraWithClipWindow::RemovedFromWidget() {
  if (host_->native_view()) {
    // Clear kHostWindowKey before Hide() because it could be accessed during
    // the call. In MUS aura, the hosting window could be destroyed at this
    // point.
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);

    host_->native_view()->Hide();
    if (host_->native_view()->parent()) {
      host_->native_view()->parent()->RemoveChild(host_->native_view());
    }
    RemoveClippingWindow();
  }
}

bool NativeViewHostAuraWithClipWindow::SetCornerRadii(
    const gfx::RoundedCornersF& corner_radii) {
  corner_radii_ = corner_radii;
  ApplyRoundedCorners();
  return true;
}

void NativeViewHostAuraWithClipWindow::SetHitTestTopInset(int top_inset) {
  if (top_inset_ == top_inset) {
    return;
  }
  top_inset_ = top_inset;
  UpdateInsets();
}

void NativeViewHostAuraWithClipWindow::InstallClip(int x, int y, int w, int h) {
  clip_rect_ = std::make_unique<gfx::Rect>(
      host_->ConvertRectToWidget(gfx::Rect(x, y, w, h)));
}

int NativeViewHostAuraWithClipWindow::GetHitTestTopInset() const {
  return top_inset_;
}

bool NativeViewHostAuraWithClipWindow::HasInstalledClip() {
  return !!clip_rect_;
}

void NativeViewHostAuraWithClipWindow::UninstallClip() {
  clip_rect_.reset();
}

bool NativeViewHostAuraWithClipWindow::SetNativeViewClipRect(
    const gfx::Rect& clip_rect) {
  std::optional<gfx::Rect> new_clip =
      clip_rect.IsEmpty() ? std::nullopt : std::make_optional(clip_rect);
  if (external_clip_rect_ == new_clip) {
    return false;
  }
  external_clip_rect_ = new_clip;
  if (host_->native_view()) {
    host_->native_view()->layer()->SetClipRect(clip_rect);
  }
  return true;
}

void NativeViewHostAuraWithClipWindow::ShowWidget(int x,
                                                  int y,
                                                  int w,
                                                  int h,
                                                  int native_w,
                                                  int native_h) {
  if (host_->fast_resize()) {
    gfx::Point origin(x, y);
    views::View::ConvertPointFromWidget(host_, &origin);
    InstallClip(origin.x(), origin.y(), w, h);
    native_w = host_->native_view()->bounds().width();
    native_h = host_->native_view()->bounds().height();
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

  clipping_window_->SetBounds(clip_rect_ ? *clip_rect_ : gfx::Rect(x, y, w, h));
  if (host_->native_view()) {
    host_->native_view()->layer()->SetClipRect(
        external_clip_rect_.value_or(gfx::Rect()));
  }

  gfx::Point clip_offset = clipping_window_->bounds().origin();
  host_->native_view()->SetBounds(
      gfx::Rect(x - clip_offset.x(), y - clip_offset.y(), native_w, native_h));
  host_->native_view()->Show();
  clipping_window_->Show();
}

void NativeViewHostAuraWithClipWindow::HideWidget() {
  host_->native_view()->Hide();
  clipping_window_->Hide();
}

void NativeViewHostAuraWithClipWindow::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client) {
    client->FocusWindow(window);
  }
}

gfx::NativeView NativeViewHostAuraWithClipWindow::GetNativeViewContainer()
    const {
  return clipping_window_.get();
}

gfx::NativeViewAccessible
NativeViewHostAuraWithClipWindow::GetNativeViewAccessible() {
  return nullptr;
}

ui::Cursor NativeViewHostAuraWithClipWindow::GetCursor(int x, int y) {
  if (host_->native_view()) {
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  }
  return ui::Cursor();
}

void NativeViewHostAuraWithClipWindow::SetVisible(bool visible) {
  if (!visible) {
    host_->native_view()->Hide();
  } else {
    host_->native_view()->Show();
  }
}

void NativeViewHostAuraWithClipWindow::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(window == host_->native_view());
  clipping_window_delegate_->set_native_view(nullptr);
}

void NativeViewHostAuraWithClipWindow::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static

void NativeViewHostAuraWithClipWindow::CreateClippingWindow() {
  clipping_window_delegate_ = std::make_unique<ClippingWindowDelegate>();
  // Use WINDOW_TYPE_CONTROLLER type so descendant views (including popups) get
  // positioned appropriately.
  clipping_window_ = std::make_unique<aura::Window>(
      clipping_window_delegate_.get(), aura::client::WINDOW_TYPE_CONTROL);
  clipping_window_->Init(ui::LAYER_NOT_DRAWN);
  clipping_window_->set_owned_by_parent(false);
  clipping_window_->SetName("NativeViewHostAuraWithClipWindowClip");
  clipping_window_->layer()->SetMasksToBounds(true);
  clipping_window_->SetProperty(views::kHostViewKey, static_cast<View*>(host_));
  UpdateInsets();
}

void NativeViewHostAuraWithClipWindow::AddClippingWindow() {
  RemoveClippingWindow();
  if (host_->GetWidget()->GetNativeView()) {
    host_->native_view()->SetProperty(
        aura::client::kHostWindowKey,
        host_->GetWidget()->GetNativeView()->GetWeakPtrAsWindow());
  } else {
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
  }
  Widget::ReparentNativeView(host_->native_view(), clipping_window_.get());
  if (host_->GetWidget()->GetNativeView()) {
    Widget::ReparentNativeView(clipping_window_.get(),
                               host_->GetWidget()->GetNativeView());
  }
}

void NativeViewHostAuraWithClipWindow::RemoveClippingWindow() {
  clipping_window_->Hide();
  if (host_->native_view()) {
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
  }

  if (host_->native_view()->parent() == clipping_window_.get()) {
    if (host_->GetWidget() && host_->GetWidget()->GetNativeView()) {
      Widget::ReparentNativeView(host_->native_view(),
                                 host_->GetWidget()->GetNativeView());
    } else {
      clipping_window_->RemoveChild(host_->native_view());
    }
  }
  if (clipping_window_->parent()) {
    clipping_window_->parent()->RemoveChild(clipping_window_.get());
  }
}

void NativeViewHostAuraWithClipWindow::ApplyRoundedCorners() {
  if (!host_->native_view()) {
    return;
  }

  ui::Layer* layer = host_->native_view()->layer();
  if (layer->rounded_corner_radii() != corner_radii_) {
    layer->SetRoundedCornerRadius(corner_radii_);
    layer->SetIsFastRoundedCorner(true);
  }
}

void NativeViewHostAuraWithClipWindow::UpdateInsets() {
  if (!clipping_window_) {
    return;
  }

  if (top_inset_ == 0) {
    // The window targeter needs to be uninstalled when not used; keeping empty
    // targeter here actually conflicts with ash::ImmersiveWindowTargeter on
    // immersive mode in Ash.
    // TODO(mukai): fix this.
    clipping_window_->SetEventTargeter(nullptr);
  } else {
    if (!clipping_window_->targeter()) {
      clipping_window_->SetEventTargeter(
          std::make_unique<aura::WindowTargeter>());
    }
    clipping_window_->targeter()->SetInsets(
        gfx::Insets::TLBR(top_inset_, 0, 0, 0));
  }
}

}  // namespace views
