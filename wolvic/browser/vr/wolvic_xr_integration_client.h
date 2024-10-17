// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WOLVIC_XR_INTEGRATION_CLIENT_H_
#define WOLVIC_BROWSER_VR_WOLVIC_XR_INTEGRATION_CLIENT_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/xr_integration_client.h"

namespace wolvic {

class WolvicContentBrowserClient;

class WolvicXrIntegrationClient : public content::XrIntegrationClient {
 public:
  explicit WolvicXrIntegrationClient(
      base::PassKey<WolvicContentBrowserClient>) {}
  ~WolvicXrIntegrationClient() override = default;
  WolvicXrIntegrationClient(const WolvicXrIntegrationClient&) = delete;
  WolvicXrIntegrationClient& operator=(const WolvicXrIntegrationClient&) =
      delete;

  // XrIntegrationClient
  std::unique_ptr<content::XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id) override;
  content::XRProviderList GetAdditionalProviders() override;
  std::unique_ptr<content::BrowserXRRuntime::Observer> CreateRuntimeObserver()
      override;

 private:
  WolvicXrIntegrationClient() = default;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WOLVIC_XR_INTEGRATION_CLIENT_H_
