// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_process.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/common/weblayer_paths.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#endif

namespace weblayer {

namespace {
BrowserProcess* g_browser_process = nullptr;
}  // namespace

BrowserProcess::BrowserProcess(std::unique_ptr<PrefService> local_state)
    : local_state_(std::move(local_state)) {
  g_browser_process = this;
}

BrowserProcess::~BrowserProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  g_browser_process = nullptr;

  SystemNetworkContextManager::DeleteInstance();
}

// static
BrowserProcess* BrowserProcess::GetInstance() {
  return g_browser_process;
}

void BrowserProcess::PreMainMessageLoopRun() {
  CreateNetworkQualityObserver();
}

void BrowserProcess::StartTearDown() {
  if (local_state_)
    local_state_->CommitPendingWrite();
}

PrefService* BrowserProcess::GetLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_state_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
BrowserProcess::GetSharedURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker* BrowserProcess::GetNetworkTimeTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_time_tracker_) {
    network_time_tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        base::WrapUnique(new base::DefaultClock()),
        base::WrapUnique(new base::DefaultTickClock()), GetLocalState(),
        GetSharedURLLoaderFactory());
  }
  return network_time_tracker_.get();
}

network::NetworkQualityTracker* BrowserProcess::GetNetworkQualityTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_quality_tracker_) {
    network_quality_tracker_ = std::make_unique<network::NetworkQualityTracker>(
        base::BindRepeating(&content::GetNetworkService));
  }
  return network_quality_tracker_.get();
}

subresource_filter::RulesetService*
BrowserProcess::subresource_filter_ruleset_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!subresource_filter_ruleset_service_)
    CreateSubresourceFilterRulesetService();
  return subresource_filter_ruleset_service_.get();
}

void BrowserProcess::CreateNetworkQualityObserver() {
  DCHECK(!network_quality_observer_);
  network_quality_observer_ =
      content::CreateNetworkQualityObserver(GetNetworkQualityTracker());
  DCHECK(network_quality_observer_);
}

void BrowserProcess::CreateSubresourceFilterRulesetService() {
  DCHECK(!subresource_filter_ruleset_service_);

  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(DIR_USER_DATA, &user_data_dir));
  subresource_filter_ruleset_service_ =
      subresource_filter::RulesetService::Create(GetLocalState(),
                                                 user_data_dir);
}

#if BUILDFLAG(IS_ANDROID)
SafeBrowsingService* BrowserProcess::GetSafeBrowsingService() {
  if (!safe_browsing_service_) {
    // Create and initialize safe_browsing_service on first get.
    // Note: Initialize() needs to happen on UI thread.
    safe_browsing_service_ =
        std::make_unique<SafeBrowsingService>(embedder_support::GetUserAgent());
    safe_browsing_service_->Initialize();
  }
  return safe_browsing_service_.get();
}

void BrowserProcess::StopSafeBrowsingService() {
  if (safe_browsing_service_) {
    safe_browsing_service_->StopDBManager();
  }
}
#endif

}  // namespace weblayer
