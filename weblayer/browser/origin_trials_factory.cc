// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/origin_trials_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace weblayer {

// static
origin_trials::OriginTrials* OriginTrialsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  if (origin_trials::features::IsPersistentOriginTrialsEnabled()) {
    return static_cast<origin_trials::OriginTrials*>(
        GetInstance()->GetServiceForBrowserContext(browser_context, true));
  }
  return nullptr;
}

// static
OriginTrialsFactory* OriginTrialsFactory::GetInstance() {
  static base::NoDestructor<OriginTrialsFactory> factory;
  return factory.get();
}

OriginTrialsFactory::OriginTrialsFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginTrials",
          BrowserContextDependencyManager::GetInstance()) {}

OriginTrialsFactory::~OriginTrialsFactory() noexcept = default;

std::unique_ptr<KeyedService>
OriginTrialsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<origin_trials::OriginTrials>(
      std::make_unique<origin_trials::LevelDbPersistenceProvider>(
          context->GetPath(),
          context->GetDefaultStoragePartition()->GetProtoDatabaseProvider()),
      std::make_unique<blink::TrialTokenValidator>());
}

content::BrowserContext* OriginTrialsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
