// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/client_side_detection_service_delegate.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

ClientSideDetectionServiceDelegate::ClientSideDetectionServiceDelegate(
    BrowserContextImpl* browser_context)
    : browser_context_(browser_context) {}

ClientSideDetectionServiceDelegate::~ClientSideDetectionServiceDelegate() =
    default;

PrefService* ClientSideDetectionServiceDelegate::GetPrefs() {
  DCHECK(browser_context_);
  return browser_context_->pref_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetURLLoaderFactory() {
  return browser_context_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

scoped_refptr<network::SharedURLLoaderFactory>
ClientSideDetectionServiceDelegate::GetSafeBrowsingURLLoaderFactory() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service ? sb_service->GetURLLoaderFactory() : nullptr;
}

safe_browsing::ChromeUserPopulation
ClientSideDetectionServiceDelegate::GetUserPopulation() {
  safe_browsing::ChromeUserPopulation population;
  if (safe_browsing::IsEnhancedProtectionEnabled(*GetPrefs())) {
    population.set_user_population(
        safe_browsing::ChromeUserPopulation::ENHANCED_PROTECTION);
  } else if (safe_browsing::IsExtendedReportingEnabled(*GetPrefs())) {
    population.set_user_population(
        safe_browsing::ChromeUserPopulation::EXTENDED_REPORTING);
  } else if (safe_browsing::IsSafeBrowsingEnabled(*GetPrefs())) {
    population.set_user_population(
        safe_browsing::ChromeUserPopulation::SAFE_BROWSING);
  }

  population.set_profile_management_status(
      safe_browsing::ChromeUserPopulation::UNAVAILABLE);
  population.set_is_mbb_enabled(GetPrefs()->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  population.set_is_incognito(browser_context_->IsOffTheRecord());
  population.set_is_history_sync_enabled(false);

  return population;
}

}  // namespace weblayer
