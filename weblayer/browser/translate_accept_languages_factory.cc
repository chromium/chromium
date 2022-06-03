// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/translate_accept_languages_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace weblayer {

// static
translate::TranslateAcceptLanguages*
TranslateAcceptLanguagesFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<translate::TranslateAcceptLanguages*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
TranslateAcceptLanguagesFactory*
TranslateAcceptLanguagesFactory::GetInstance() {
  static base::NoDestructor<TranslateAcceptLanguagesFactory> factory;
  return factory.get();
}

TranslateAcceptLanguagesFactory::TranslateAcceptLanguagesFactory()
    : BrowserContextKeyedServiceFactory(
          "TranslateAcceptLanguages",
          BrowserContextDependencyManager::GetInstance()) {}

TranslateAcceptLanguagesFactory::~TranslateAcceptLanguagesFactory() = default;

KeyedService* TranslateAcceptLanguagesFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new translate::TranslateAcceptLanguages(
      user_prefs::UserPrefs::Get(browser_context),
      language::prefs::kAcceptLanguages);
}

content::BrowserContext*
TranslateAcceptLanguagesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
