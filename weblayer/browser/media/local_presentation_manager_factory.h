// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_MEDIA_LOCAL_PRESENTATION_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_MEDIA_LOCAL_PRESENTATION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

// A version of LocalPresentationManagerFactory that does not redirect from
// incognito to normal context.
class LocalPresentationManagerFactory
    : public media_router::LocalPresentationManagerFactory {
 public:
  LocalPresentationManagerFactory(const LocalPresentationManagerFactory&) =
      delete;
  LocalPresentationManagerFactory& operator=(
      const LocalPresentationManagerFactory&) = delete;

  static LocalPresentationManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<LocalPresentationManagerFactory>;

  LocalPresentationManagerFactory();
  ~LocalPresentationManagerFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_MEDIA_LOCAL_PRESENTATION_MANAGER_FACTORY_H_
