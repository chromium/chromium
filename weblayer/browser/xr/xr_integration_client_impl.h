// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_XR_XR_INTEGRATION_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_XR_XR_INTEGRATION_CLIENT_IMPL_H_

#include <memory>

#include "content/public/browser/xr_integration_client.h"

namespace weblayer {

class XrIntegrationClientImpl : public content::XrIntegrationClient {
 public:
  XrIntegrationClientImpl() = default;
  ~XrIntegrationClientImpl() override = default;
  XrIntegrationClientImpl(const XrIntegrationClientImpl&) = delete;
  XrIntegrationClientImpl& operator=(const XrIntegrationClientImpl&) = delete;

  // Returns whether XR should be enabled.
  static bool IsEnabled();

  // content::XrIntegrationClient:
  std::unique_ptr<content::XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id) override;
  content::XRProviderList GetAdditionalProviders() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_XR_XR_INTEGRATION_CLIENT_IMPL_H_
