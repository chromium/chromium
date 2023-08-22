// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/safe_browsing/weblayer_client_side_detection_service_delegate.h"
#include "weblayer/common/features.h"

namespace weblayer {

// static
safe_browsing::ClientSideDetectionService*
ClientSideDetectionServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  if (base::FeatureList::IsEnabled(
          features::kWebLayerClientSidePhishingDetection)) {
    return static_cast<safe_browsing::ClientSideDetectionService*>(
        GetInstance()->GetServiceForBrowserContext(browser_context,
                                                   /* create= */ true));
  }
  return nullptr;
}

// static
ClientSideDetectionServiceFactory*
ClientSideDetectionServiceFactory::GetInstance() {
  static base::NoDestructor<ClientSideDetectionServiceFactory> factory;
  return factory.get();
}

ClientSideDetectionServiceFactory::ClientSideDetectionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientSideDetectionService",
          BrowserContextDependencyManager::GetInstance()) {}

ClientSideDetectionServiceFactory::~ClientSideDetectionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ClientSideDetectionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<safe_browsing::ClientSideDetectionService>(
      std::make_unique<WebLayerClientSideDetectionServiceDelegate>(
          static_cast<BrowserContextImpl*>(context)),
      /*opt_guide=*/nullptr,
      /*background_task_runner=*/nullptr);
}

content::BrowserContext*
ClientSideDetectionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
