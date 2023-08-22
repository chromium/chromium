// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/reduce_accept_language_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
reduce_accept_language::ReduceAcceptLanguageService*
ReduceAcceptLanguageFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<reduce_accept_language::ReduceAcceptLanguageService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ReduceAcceptLanguageFactory* ReduceAcceptLanguageFactory::GetInstance() {
  static base::NoDestructor<ReduceAcceptLanguageFactory> instance;
  return instance.get();
}

ReduceAcceptLanguageFactory::ReduceAcceptLanguageFactory()
    : BrowserContextKeyedServiceFactory(
          "ReduceAcceptLanguage",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

ReduceAcceptLanguageFactory::~ReduceAcceptLanguageFactory() = default;

std::unique_ptr<KeyedService>
ReduceAcceptLanguageFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<reduce_accept_language::ReduceAcceptLanguageService>(
      HostContentSettingsMapFactory::GetForBrowserContext(context),
      static_cast<BrowserContextImpl*>(context)->pref_service(),
      context->IsOffTheRecord());
}

content::BrowserContext* ReduceAcceptLanguageFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
