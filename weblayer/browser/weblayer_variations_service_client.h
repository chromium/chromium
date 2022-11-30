// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_
#define WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/variations/service/variations_service_client.h"

namespace weblayer {

class SystemNetworkContextManager;

// WebLayerVariationsServiceClient provides an implementation of
// VariationsServiceClient, all members are currently stubs for WebLayer.
class WebLayerVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  explicit WebLayerVariationsServiceClient(
      SystemNetworkContextManager* network_context_manager);

  WebLayerVariationsServiceClient(const WebLayerVariationsServiceClient&) =
      delete;
  WebLayerVariationsServiceClient& operator=(
      const WebLayerVariationsServiceClient&) = delete;

  ~WebLayerVariationsServiceClient() override;

 private:
  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  version_info::Channel GetChannel() override;
  bool OverridesRestrictParameter(std::string* parameter) override;
  bool IsEnterprise() override;

  raw_ptr<SystemNetworkContextManager> network_context_manager_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_VARIATIONS_SERVICE_CLIENT_H_
