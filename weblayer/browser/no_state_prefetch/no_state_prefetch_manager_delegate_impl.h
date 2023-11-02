// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class NoStatePrefetchManagerDelegateImpl
    : public prerender::NoStatePrefetchManagerDelegate {
 public:
  explicit NoStatePrefetchManagerDelegateImpl(
      content::BrowserContext* browser_context);
  ~NoStatePrefetchManagerDelegateImpl() override = default;

  // NoStatePrefetchManagerDelegate overrides.
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings() override;
  std::unique_ptr<prerender::NoStatePrefetchContentsDelegate>
  GetNoStatePrefetchContentsDelegate() override;
  bool IsNetworkPredictionPreferenceEnabled() override;
  std::string GetReasonForDisablingPrediction() override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_DELEGATE_IMPL_H_
