// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_
#define WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_

#include <memory>

#include "components/permissions/bluetooth_chooser_controller.h"
#include "components/permissions/bluetooth_delegate_impl.h"
#include "content/public/browser/bluetooth_delegate.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace permissions {
class BluetoothChooserContext;
}  // namespace permissions

namespace weblayer {

// Provides embedder-level functionality to BluetoothDelegateImpl in WebLayer.
class WebLayerBluetoothDelegateImplClient
    : public permissions::BluetoothDelegateImpl::Client {
 public:
  WebLayerBluetoothDelegateImplClient();
  ~WebLayerBluetoothDelegateImplClient() override;

  WebLayerBluetoothDelegateImplClient(
      const WebLayerBluetoothDelegateImplClient&) = delete;
  WebLayerBluetoothDelegateImplClient& operator=(
      const WebLayerBluetoothDelegateImplClient&) = delete;

  // BluetoothDelegateImpl::Client implementation:
  permissions::BluetoothChooserContext* GetBluetoothChooserContext(
      content::RenderFrameHost* frame) override;
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;

  void ShowBluetoothDevicePairDialog(
      content::RenderFrameHost* frame,
      const std::u16string& device_identifier,
      content::BluetoothDelegate::PairPromptCallback callback,
      content::BluetoothDelegate::PairingKind,
      const absl::optional<std::u16string>& pin) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BLUETOOTH_WEBLAYER_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_
