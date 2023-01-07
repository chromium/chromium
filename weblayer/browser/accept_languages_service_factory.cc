// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/accept_languages_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace weblayer {

// static
language::AcceptLanguagesService*
AcceptLanguagesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<language::AcceptLanguagesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  static base::NoDestructor<AcceptLanguagesServiceFactory> factory;
  return factory.get();
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AcceptLanguagesService",
          BrowserContextDependencyManager::GetInstance()) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() = default;

KeyedService* AcceptLanguagesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new language::AcceptLanguagesService(
      user_prefs::UserPrefs::Get(browser_context),
      language::prefs::kAcceptLanguages);
}

content::BrowserContext* AcceptLanguagesServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
