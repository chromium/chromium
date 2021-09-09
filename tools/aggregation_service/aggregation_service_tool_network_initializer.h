// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_NETWORK_INITIALIZER_H_
#define TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_NETWORK_INITIALIZER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/public/test/test_aggregation_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace aggregation_service {

// This class is responsible for initializing network states. The object should
// be kept alive for the duration of network usage.
class ToolNetworkInitializer {
 public:
  // `agg_service` must be a non-null pointer to a TestAggregationService.
  explicit ToolNetworkInitializer(content::TestAggregationService* agg_service);
  ToolNetworkInitializer(const ToolNetworkInitializer& other) = delete;
  ToolNetworkInitializer& operator=(const ToolNetworkInitializer& other) =
      delete;
  ~ToolNetworkInitializer();

 private:
  std::unique_ptr<network::NetworkService> network_service_;
  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

}  // namespace aggregation_service

#endif  // TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_NETWORK_INITIALIZER_H_