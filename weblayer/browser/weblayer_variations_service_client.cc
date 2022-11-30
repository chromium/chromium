// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_variations_service_client.h"

#include "build/build_config.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/system_network_context_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/version_info/android/channel_getter.h"
#endif

using version_info::Channel;

namespace weblayer {

WebLayerVariationsServiceClient::WebLayerVariationsServiceClient(
    SystemNetworkContextManager* network_context_manager)
    : network_context_manager_(network_context_manager) {
  DCHECK(network_context_manager_);
}

WebLayerVariationsServiceClient::~WebLayerVariationsServiceClient() = default;

base::Version WebLayerVariationsServiceClient::GetVersionForSimulation() {
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebLayerVariationsServiceClient::GetURLLoaderFactory() {
  return network_context_manager_->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker*
WebLayerVariationsServiceClient::GetNetworkTimeTracker() {
  return BrowserProcess::GetInstance()->GetNetworkTimeTracker();
}

Channel WebLayerVariationsServiceClient::GetChannel() {
#if BUILDFLAG(IS_ANDROID)
  return version_info::android::GetChannel();
#else
  return version_info::Channel::UNKNOWN;
#endif
}

bool WebLayerVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

bool WebLayerVariationsServiceClient::IsEnterprise() {
  return false;
}

}  // namespace weblayer
