// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"

#include "components/safe_browsing/core/browser/user_population.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/verdict_cache_manager_factory.h"

namespace weblayer {

safe_browsing::ChromeUserPopulation GetUserPopulationForBrowserContext(
    content::BrowserContext* browser_context) {
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);

  return safe_browsing::GetUserPopulation(
      browser_context_impl->pref_service(),
      browser_context_impl->IsOffTheRecord(),
      /*is_history_sync_active=*/false,
      /*is_signed_in=*/false,
      /*is_under_advanced_protection=*/false,
      /*browser_policy_connector=*/nullptr,
      /*num_profiles=*/absl::optional<size_t>(),
      /*num_loaded_profiles=*/absl::optional<size_t>(),
      /*num_open_profiles=*/absl::optional<size_t>());
}

safe_browsing::ChromeUserPopulation::PageLoadToken GetPageLoadTokenForURL(
    content::BrowserContext* browser_context,
    GURL url) {
  safe_browsing::VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForBrowserContext(browser_context);
  if (!cache_manager) {
    return safe_browsing::ChromeUserPopulation::PageLoadToken();
  }
  return cache_manager->GetPageLoadToken(url);
}

}  // namespace weblayer
