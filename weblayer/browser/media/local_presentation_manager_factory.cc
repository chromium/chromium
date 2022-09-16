// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/media/local_presentation_manager_factory.h"

#include "base/no_destructor.h"

namespace weblayer {

// static
LocalPresentationManagerFactory*
LocalPresentationManagerFactory::GetInstance() {
  static base::NoDestructor<LocalPresentationManagerFactory> instance;
  return instance.get();
}

LocalPresentationManagerFactory::LocalPresentationManagerFactory() = default;
LocalPresentationManagerFactory::~LocalPresentationManagerFactory() = default;

content::BrowserContext*
LocalPresentationManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
