// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {

class NativeViewHost;

// Aura implementation of NativeViewHostWrapper.
class NativeViewHostAura : public NativeViewHostWrapper,
                           public aura::WindowObserver {
 public:
  explicit NativeViewHostAura(NativeViewHost* host);
  ~NativeViewHostAura() override;

  // Overridden from NativeViewHostWrapper:
  void AttachNativeView() override;
  void NativeViewDetaching(bool destroyed) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool SetCustomMask(std::unique_ptr<ui::LayerOwner> mask) override;
  void SetHitTestTopInset(int top_inset) override;
  int GetHitTestTopInset() const override;
  void InstallClip(int x, int y, int w, int h) override;
  bool HasInstalledClip() override;
  void UninstallClip() override;
  void ShowWidget(int x, int y, int w, int h, int native_w, int native_h)
      override;
  void HideWidget() override;
  void SetFocus() override;
  gfx::NativeView GetNativeViewContainer() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::NativeCursor GetCursor(int x, int y) override;
  void SetVisible(bool visible) override;
  void SetParentAccessible(gfx::NativeViewAccessible) override;

 private:
  friend class NativeViewHostAuraTest;
  class ClippingWindowDelegate;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  void CreateClippingWindow();

  // Reparents the native view with the clipping window existing between it and
  // its old parent, so that the fast resize path works.
  void AddClippingWindow();

  // If the native view has been reparented via AddClippingWindow, this call
  // undoes it.
  void RemoveClippingWindow();

  // Sets or updates the mask layer on the native view's layer.
  void InstallMask();

  // Unsets the mask layer on the native view's layer.
  void UninstallMask();

  // Updates the top insets of |clipping_window_|.
  void UpdateInsets();

  // Our associated NativeViewHost.
  NativeViewHost* host_;

  std::unique_ptr<ClippingWindowDelegate> clipping_window_delegate_;

  // Window that exists between the native view and the parent that allows for
  // clipping to occur. This is positioned in the coordinate space of
  // host_->GetWidget().
  std::unique_ptr<aura::Window> clipping_window_;
  std::unique_ptr<gfx::Rect> clip_rect_;

  // This mask exists for the sake of SetCornerRadius().
  std::unique_ptr<ui::LayerOwner> mask_;

  // Set when AttachNativeView() is called. This is the original transform of
  // the NativeView's layer. The NativeView's layer may be modified to scale
  // when ShowWidget() is called with a native view size not equal to the
  // region's size. When NativeViewDetaching() is called, the NativeView's
  // transform is restored to this.
  gfx::Transform original_transform_;

  // True if a transform different from the original was set.
  bool original_transform_changed_ = false;

  // The top insets to exclude the underlying native view from the target.
  int top_inset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
