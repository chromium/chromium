// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_delegate_impl.h"

#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/profile.h"

namespace weblayer {

NoStatePrefetchManagerDelegateImpl::NoStatePrefetchManagerDelegateImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

scoped_refptr<content_settings::CookieSettings>
NoStatePrefetchManagerDelegateImpl::GetCookieSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return CookieSettingsFactory::GetForBrowserContext(browser_context_);
}

std::unique_ptr<prerender::NoStatePrefetchContentsDelegate>
NoStatePrefetchManagerDelegateImpl::GetNoStatePrefetchContentsDelegate() {
  return std::make_unique<prerender::NoStatePrefetchContentsDelegate>();
}

bool NoStatePrefetchManagerDelegateImpl::
    IsNetworkPredictionPreferenceEnabled() {
  auto* profile = ProfileImpl::FromBrowserContext(browser_context_);
  DCHECK(profile);

  return profile->GetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED);
}

std::string
NoStatePrefetchManagerDelegateImpl::GetReasonForDisablingPrediction() {
  return IsNetworkPredictionPreferenceEnabled() ? ""
                                                : "Disabled by user setting";
}

}  // namespace weblayer
