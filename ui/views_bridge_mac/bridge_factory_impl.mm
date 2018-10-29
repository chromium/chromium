// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_bridge_mac/bridge_factory_impl.h"

#include "base/no_destructor.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/views_bridge_mac/bridged_native_widget_host_helper.h"
#include "ui/views_bridge_mac/bridged_native_widget_impl.h"

namespace views_bridge_mac {

using views::BridgedNativeWidgetImpl;
using views::BridgedNativeWidgetHostHelper;

namespace {

class Bridge : public BridgedNativeWidgetHostHelper {
 public:
  Bridge(uint64_t bridge_id,
         mojom::BridgedNativeWidgetHostAssociatedPtrInfo host_ptr,
         mojom::BridgedNativeWidgetAssociatedRequest bridge_request) {
    host_ptr_.Bind(std::move(host_ptr),
                   ui::WindowResizeHelperMac::Get()->task_runner());
    bridge_impl_ = std::make_unique<BridgedNativeWidgetImpl>(
        bridge_id, host_ptr_.get(), this);
    bridge_impl_->BindRequest(
        std::move(bridge_request),
        base::BindOnce(&Bridge::OnConnectionError, base::Unretained(this)));
  }

 private:
  ~Bridge() override {}

  void OnConnectionError() { delete this; }

  // BridgedNativeWidgetHostHelper:
  NSView* GetNativeViewAccessible() override { return nil; }
  void DispatchKeyEvent(ui::KeyEvent* event) override {}
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override {
    return false;
  }
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override {
    *found_word = false;
  }
  double SheetPositionY() override { return 0; }
  views_bridge_mac::DragDropClient* GetDragDropClient() override {
    // Drag-drop only doesn't work across mojo yet.
    return nullptr;
  }

  mojom::BridgedNativeWidgetHostAssociatedPtr host_ptr_;
  std::unique_ptr<BridgedNativeWidgetImpl> bridge_impl_;
};

}  // namespace

// static
BridgeFactoryImpl* BridgeFactoryImpl::Get() {
  static base::NoDestructor<BridgeFactoryImpl> factory;
  return factory.get();
}

void BridgeFactoryImpl::BindRequest(
    mojom::BridgeFactoryAssociatedRequest request) {
  binding_.Bind(std::move(request),
                ui::WindowResizeHelperMac::Get()->task_runner());
}

void BridgeFactoryImpl::CreateBridgedNativeWidget(
    uint64_t bridge_id,
    mojom::BridgedNativeWidgetAssociatedRequest bridge_request,
    mojom::BridgedNativeWidgetHostAssociatedPtrInfo host) {
  // The resulting object will be destroyed when its message pipe is closed.
  ignore_result(
      new Bridge(bridge_id, std::move(host), std::move(bridge_request)));
}

BridgeFactoryImpl::BridgeFactoryImpl() : binding_(this) {}

BridgeFactoryImpl::~BridgeFactoryImpl() {}

}  // namespace views_bridge_mac
