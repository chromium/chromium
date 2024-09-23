// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/cocoa/views_hostable.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace ui {
class ViewsHostableView;
}  // namespace ui

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {

class NativeWidgetMacNSWindowHost;
class NativeViewHost;

// Mac implementation of NativeViewHostWrapper.
class NativeViewHostMac : public NativeViewHostWrapper,
                          public ui::ViewsHostableView::Host {
 public:
  explicit NativeViewHostMac(NativeViewHost* host);

  NativeViewHostMac(const NativeViewHostMac&) = delete;
  NativeViewHostMac& operator=(const NativeViewHostMac&) = delete;

  ~NativeViewHostMac() override;

  // ViewsHostableView::Host:
  ui::Layer* GetUiLayer() const override;
  remote_cocoa::mojom::Application* GetRemoteCocoaApplication() const override;
  uint64_t GetNSViewId() const override;
  void OnHostableViewDestroying() override;

  // NativeViewHostWrapper:
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
  // Return the NativeWidgetMacNSWindowHost for this hosted view.
  NativeWidgetMacNSWindowHost* GetNSWindowHost() const;

  // Our associated NativeViewHost. Owns this.
  raw_ptr<NativeViewHost> host_;

  // Retain the native view as it may be destroyed at an unpredictable time.
  NSView* __strong native_view_;

  // If |native_view| supports the ViewsHostable protocol, then this is the
  // the corresponding ViewsHostableView interface (which is implemeted only
  // by WebContents and tests).
  raw_ptr<ui::ViewsHostableView> native_view_hostable_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_
