// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_BRIDGE_FACTORY_IMPL_H_
#define UI_VIEWS_BRIDGE_MAC_BRIDGE_FACTORY_IMPL_H_

#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/views/views_export.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget.mojom.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"

// TODO(ccameron): This file is to be moved to /ui/views_bridge_mac when
// possible. For now, put it in the namespace of that path.
namespace views_bridge_mac {

// The factory that creates BridgedNativeWidget instances. This object is to
// be instantiated in app shim processes.
class VIEWS_EXPORT BridgeFactoryImpl : public mojom::BridgeFactory {
 public:
  static BridgeFactoryImpl* Get();
  void BindRequest(mojom::BridgeFactoryAssociatedRequest request);

  // mojom::BridgeFactory:
  void CreateBridgedNativeWidget(
      uint64_t bridge_id,
      mojom::BridgedNativeWidgetAssociatedRequest bridge_request,
      mojom::BridgedNativeWidgetHostAssociatedPtrInfo host) override;

 private:
  friend class base::NoDestructor<BridgeFactoryImpl>;
  BridgeFactoryImpl();
  ~BridgeFactoryImpl() override;

  mojo::AssociatedBinding<mojom::BridgeFactory> binding_;
};

}  // namespace views_bridge_mac

#endif  // UI_VIEWS_BRIDGE_MAC_BRIDGE_FACTORY_IMPL_H_
