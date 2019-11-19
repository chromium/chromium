// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/base/cocoa/views_hostable.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace ui {
class LayerOwner;
class ViewsHostableView;
}  // namespace ui

namespace views {

class NativeWidgetMacNSWindowHost;
class NativeViewHost;

// Mac implementation of NativeViewHostWrapper.
class NativeViewHostMac : public NativeViewHostWrapper,
                          public ui::ViewsHostableView::Host {
 public:
  explicit NativeViewHostMac(NativeViewHost* host);
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
  // Return the NativeWidgetMacNSWindowHost for this hosted view.
  NativeWidgetMacNSWindowHost* GetNSWindowHost() const;

  // Our associated NativeViewHost. Owns this.
  NativeViewHost* host_;

  // Retain the native view as it may be destroyed at an unpredictable time.
  base::scoped_nsobject<NSView> native_view_;

  // If |native_view| supports the ViewsHostable protocol, then this is the
  // the corresponding ViewsHostableView interface (which is implemeted only
  // by WebContents and tests).
  ui::ViewsHostableView* native_view_hostable_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_
