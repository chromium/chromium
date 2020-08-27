// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_DELEGATE_IMPL_H_

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/prerender/browser/prerender_manager_delegate.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class PrerenderManagerDelegateImpl
    : public prerender::PrerenderManagerDelegate {
 public:
  explicit PrerenderManagerDelegateImpl(
      content::BrowserContext* browser_context);
  ~PrerenderManagerDelegateImpl() override = default;

  // PrerenderManagerDelegate overrides.
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings() override;
  std::unique_ptr<prerender::PrerenderContentsDelegate>
  GetPrerenderContentsDelegate() override;
  bool IsNetworkPredictionPreferenceEnabled() override;
  std::string GetReasonForDisablingPrediction() override;

 private:
  content::BrowserContext* browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_MANAGER_DELEGATE_IMPL_H_
