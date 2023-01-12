// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/web_data_service_factory.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/i18n_util.h"

namespace weblayer {

namespace {

// Callback to show error dialog on profile load error.
void ProfileErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                          sql::InitStatus status,
                          const std::string& diagnostics) {
  NOTIMPLEMENTED();
}

}  // namespace

WebDataServiceFactory::WebDataServiceFactory() = default;

WebDataServiceFactory::~WebDataServiceFactory() = default;

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  static base::NoDestructor<WebDataServiceFactory> instance;
  return instance.get();
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

KeyedService* WebDataServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  const base::FilePath& profile_path = context->GetPath();
  return new WebDataServiceWrapper(profile_path, i18n::GetApplicationLocale(),
                                   content::GetUIThreadTaskRunner({}),
                                   base::BindRepeating(&ProfileErrorCallback));
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace weblayer
