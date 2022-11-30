// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"

#include "components/safe_browsing/core/browser/user_population.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

safe_browsing::ChromeUserPopulation GetUserPopulationForBrowserContext(
    content::BrowserContext* browser_context) {
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);

  return safe_browsing::GetUserPopulation(
      browser_context_impl->pref_service(),
      browser_context_impl->IsOffTheRecord(),
      /*is_history_sync_enabled=*/false,
      /*is_signed_in=*/false,
      /*is_under_advanced_protection=*/false,
      /*browser_policy_connector=*/nullptr,
      /*num_profiles=*/absl::optional<size_t>(),
      /*num_loaded_profiles=*/absl::optional<size_t>(),
      /*num_open_profiles=*/absl::optional<size_t>());
}

}  // namespace weblayer
