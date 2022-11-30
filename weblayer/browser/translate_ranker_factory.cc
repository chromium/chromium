// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/translate_ranker_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace weblayer {

// static
translate::TranslateRanker* TranslateRankerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<translate::TranslateRanker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
TranslateRankerFactory* TranslateRankerFactory::GetInstance() {
  static base::NoDestructor<TranslateRankerFactory> factory;
  return factory.get();
}

TranslateRankerFactory::TranslateRankerFactory()
    : BrowserContextKeyedServiceFactory(
          "TranslateRanker",
          BrowserContextDependencyManager::GetInstance()) {}

TranslateRankerFactory::~TranslateRankerFactory() = default;

KeyedService* TranslateRankerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new translate::TranslateRankerImpl(
      translate::TranslateRankerImpl::GetModelPath(browser_context->GetPath()),
      translate::TranslateRankerImpl::GetModelURL(), ukm::UkmRecorder::Get());
}

content::BrowserContext* TranslateRankerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
