// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {

class NativeViewHost;

// Aura implementation of NativeViewHostWrapper.
class NativeViewHostAura : public NativeViewHostWrapper,
                           public aura::WindowObserver {
 public:
  explicit NativeViewHostAura(NativeViewHost* host);

  NativeViewHostAura(const NativeViewHostAura&) = delete;
  NativeViewHostAura& operator=(const NativeViewHostAura&) = delete;

  ~NativeViewHostAura() override;

  // Overridden from NativeViewHostWrapper:
  void AttachNativeView() override;
  void NativeViewDetaching(bool destroyed) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool SetCornerRadii(const gfx::RoundedCornersF& corner_radii) override;
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
  ui::Cursor GetCursor(int x, int y) override;
  void SetVisible(bool visible) override;
  void SetParentAccessible(gfx::NativeViewAccessible) override;
  gfx::NativeViewAccessible GetParentAccessible() override;
  ui::Layer* GetUILayer() override;

 private:
  friend class NativeViewHostAuraTest;
  class ClippingWindowDelegate;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;

  void CreateClippingWindow();

  // Reparents the native view with the clipping window existing between it and
  // its old parent, so that the fast resize path works.
  void AddClippingWindow();

  // If the native view has been reparented via AddClippingWindow, this call
  // undoes it.
  void RemoveClippingWindow();

  // Sets or updates the |corner_radii_| on the native view's layer.
  void ApplyRoundedCorners();

  // Updates the top insets of |clipping_window_|.
  void UpdateInsets();

  // Our associated NativeViewHost.
  raw_ptr<NativeViewHost> host_;

  std::unique_ptr<ClippingWindowDelegate> clipping_window_delegate_;

  // Window that exists between the native view and the parent that allows for
  // clipping to occur. This is positioned in the coordinate space of
  // host_->GetWidget().
  std::unique_ptr<aura::Window> clipping_window_;
  std::unique_ptr<gfx::Rect> clip_rect_;

  // Holds the corner_radii to be applied.
  gfx::RoundedCornersF corner_radii_;

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
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
