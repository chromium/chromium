// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
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

class NativeViewHostAura::ClippingWindowDelegate : public aura::WindowDelegate {
 public:
  ClippingWindowDelegate() = default;
  ~ClippingWindowDelegate() override = default;

  void set_native_view(aura::Window* native_view) {
    native_view_ = native_view;
  }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
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

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host) : host_(host) {}

NativeViewHostAura::~NativeViewHostAura() {
  if (host_->native_view()) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    clipping_window_->ClearProperty(views::kHostViewKey);
    if (host_->native_view()->parent() == clipping_window_.get())
      clipping_window_->RemoveChild(host_->native_view());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::AttachNativeView() {
  if (!clipping_window_)
    CreateClippingWindow();
  clipping_window_delegate_->set_native_view(host_->native_view());
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
                                    static_cast<View*>(host_));

  original_transform_ = host_->native_view()->transform();
  original_transform_changed_ = false;
  AddClippingWindow();
  ApplyRoundedCorners();
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
  return host_->native_view()->layer();
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  // This method causes a succession of window tree changes. ScopedPause ensures
  // that occlusion is recomputed at the end of the method instead of after each
  // change.
  std::optional<aura::WindowOcclusionTracker::ScopedPause> pause_occlusion;
  if (clipping_window_)
    pause_occlusion.emplace();

  clipping_window_delegate_->set_native_view(nullptr);
  RemoveClippingWindow();
  if (!destroyed) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->ClearProperty(
        aura::client::kParentNativeViewAccessibleKey);
    if (original_transform_changed_)
      host_->native_view()->SetTransform(original_transform_);
    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      Widget::ReparentNativeView(host_->native_view(), nullptr);
  }
}

void NativeViewHostAura::AddedToWidget() {
  if (!host_->native_view())
    return;

  AddClippingWindow();
  if (host_->IsDrawn())
    host_->native_view()->Show();
  else
    host_->native_view()->Hide();
  host_->InvalidateLayout();
}

void NativeViewHostAura::RemovedFromWidget() {
  if (host_->native_view()) {
    // Clear kHostWindowKey before Hide() because it could be accessed during
    // the call. In MUS aura, the hosting window could be destroyed at this
    // point.
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);

    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      host_->native_view()->parent()->RemoveChild(host_->native_view());
    RemoveClippingWindow();
  }
}

bool NativeViewHostAura::SetCornerRadii(
    const gfx::RoundedCornersF& corner_radii) {
  corner_radii_ = corner_radii;
  ApplyRoundedCorners();
  return true;
}

void NativeViewHostAura::SetHitTestTopInset(int top_inset) {
  if (top_inset_ == top_inset)
    return;
  top_inset_ = top_inset;
  UpdateInsets();
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  clip_rect_ = std::make_unique<gfx::Rect>(
      host_->ConvertRectToWidget(gfx::Rect(x, y, w, h)));
}

int NativeViewHostAura::GetHitTestTopInset() const {
  return top_inset_;
}

bool NativeViewHostAura::HasInstalledClip() {
  return !!clip_rect_;
}

void NativeViewHostAura::UninstallClip() {
  clip_rect_.reset();
}

void NativeViewHostAura::ShowWidget(int x,
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
  gfx::Point clip_offset = clipping_window_->bounds().origin();
  host_->native_view()->SetBounds(
      gfx::Rect(x - clip_offset.x(), y - clip_offset.y(), native_w, native_h));
  host_->native_view()->Show();
  clipping_window_->Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
  clipping_window_->Hide();
}

void NativeViewHostAura::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client)
    client->FocusWindow(window);
}

gfx::NativeView NativeViewHostAura::GetNativeViewContainer() const {
  return clipping_window_.get();
}

gfx::NativeViewAccessible NativeViewHostAura::GetNativeViewAccessible() {
  return nullptr;
}

ui::Cursor NativeViewHostAura::GetCursor(int x, int y) {
  if (host_->native_view())
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  return ui::Cursor();
}

void NativeViewHostAura::SetVisible(bool visible) {
  if (!visible)
    host_->native_view()->Hide();
  else
    host_->native_view()->Show();
}

void NativeViewHostAura::OnWindowDestroying(aura::Window* window) {
  DCHECK(window == host_->native_view());
  clipping_window_delegate_->set_native_view(nullptr);
}

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostAura(host);
}

void NativeViewHostAura::CreateClippingWindow() {
  clipping_window_delegate_ = std::make_unique<ClippingWindowDelegate>();
  // Use WINDOW_TYPE_CONTROLLER type so descendant views (including popups) get
  // positioned appropriately.
  clipping_window_ = std::make_unique<aura::Window>(
      clipping_window_delegate_.get(), aura::client::WINDOW_TYPE_CONTROL);
  clipping_window_->Init(ui::LAYER_NOT_DRAWN);
  clipping_window_->set_owned_by_parent(false);
  clipping_window_->SetName("NativeViewHostAuraClip");
  clipping_window_->layer()->SetMasksToBounds(true);
  clipping_window_->SetProperty(views::kHostViewKey, static_cast<View*>(host_));
  UpdateInsets();
}

void NativeViewHostAura::AddClippingWindow() {
  RemoveClippingWindow();

  host_->native_view()->SetProperty(aura::client::kHostWindowKey,
                                    host_->GetWidget()->GetNativeView());
  Widget::ReparentNativeView(host_->native_view(), clipping_window_.get());
  if (host_->GetWidget()->GetNativeView()) {
    Widget::ReparentNativeView(clipping_window_.get(),
                               host_->GetWidget()->GetNativeView());
  }
}

void NativeViewHostAura::RemoveClippingWindow() {
  clipping_window_->Hide();
  if (host_->native_view())
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);

  if (host_->native_view()->parent() == clipping_window_.get()) {
    if (host_->GetWidget() && host_->GetWidget()->GetNativeView()) {
      Widget::ReparentNativeView(host_->native_view(),
                                 host_->GetWidget()->GetNativeView());
    } else {
      clipping_window_->RemoveChild(host_->native_view());
    }
  }
  if (clipping_window_->parent())
    clipping_window_->parent()->RemoveChild(clipping_window_.get());
}

void NativeViewHostAura::ApplyRoundedCorners() {
  if (!host_->native_view())
    return;

  ui::Layer* layer = host_->native_view()->layer();
  if (layer->rounded_corner_radii() != corner_radii_) {
    layer->SetRoundedCornerRadius(corner_radii_);
    layer->SetIsFastRoundedCorner(true);
  }
}

void NativeViewHostAura::UpdateInsets() {
  if (!clipping_window_)
    return;

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
