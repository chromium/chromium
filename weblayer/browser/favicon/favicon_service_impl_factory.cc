// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/favicon_service_impl_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/core_favicon_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"

namespace weblayer {

namespace {

favicon::LargeFaviconProvider* GetLargeFaviconProvider(
    content::BrowserContext* browser_context) {
  return FaviconServiceImplFactory::GetForBrowserContext(browser_context);
}

}  // namespace

// static
FaviconServiceImpl* FaviconServiceImplFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  if (!browser_context->IsOffTheRecord()) {
    return static_cast<FaviconServiceImpl*>(
        GetInstance()->GetServiceForBrowserContext(browser_context, true));
  }
  return nullptr;
}

// static
FaviconServiceImplFactory* FaviconServiceImplFactory::GetInstance() {
  static base::NoDestructor<FaviconServiceImplFactory> factory;
  return factory.get();
}

FaviconServiceImplFactory::FaviconServiceImplFactory()
    : BrowserContextKeyedServiceFactory(
          "FaviconServiceImpl",
          BrowserContextDependencyManager::GetInstance()) {
  favicon::SetLargeFaviconProviderGetter(
      base::BindRepeating(&GetLargeFaviconProvider));
}

FaviconServiceImplFactory::~FaviconServiceImplFactory() = default;

KeyedService* FaviconServiceImplFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  std::unique_ptr<FaviconServiceImpl> service =
      std::make_unique<FaviconServiceImpl>();
  service->Init(context->GetPath().AppendASCII("Favicons"));
  return service.release();
}

bool FaviconServiceImplFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace weblayer
