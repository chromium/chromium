// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/feature_list_creator.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/weblayer_variations_service_client.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#endif

#if BUILDFLAG(IS_ANDROID)
namespace switches {
const char kDisableBackgroundNetworking[] = "disable-background-networking";
}  // namespace switches
#endif

namespace weblayer {
namespace {

FeatureListCreator* feature_list_creator_instance = nullptr;

}  // namespace

FeatureListCreator::FeatureListCreator(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  DCHECK(!feature_list_creator_instance);
  feature_list_creator_instance = this;
}

FeatureListCreator::~FeatureListCreator() {
  feature_list_creator_instance = nullptr;
}

// static
FeatureListCreator* FeatureListCreator::GetInstance() {
  DCHECK(feature_list_creator_instance);
  return feature_list_creator_instance;
}

void FeatureListCreator::SetSystemNetworkContextManager(
    SystemNetworkContextManager* system_network_context_manager) {
  system_network_context_manager_ = system_network_context_manager;
}

void FeatureListCreator::CreateFeatureListAndFieldTrials() {
#if BUILDFLAG(IS_ANDROID)
  WebLayerMetricsServiceClient::GetInstance()->Initialize(local_state_);
#endif
  SetUpFieldTrials();
}

void FeatureListCreator::PerformPreMainMessageLoopStartup() {
#if BUILDFLAG(IS_ANDROID)
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);
  variations_service_->PerformPreMainMessageLoopStartup();
#endif
}

void FeatureListCreator::OnBrowserFragmentStarted() {
  if (has_browser_fragment_started_)
    return;

  has_browser_fragment_started_ = true;
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);

  // This function is called any time a BrowserFragment is started.
  // OnAppEnterForeground() really need only be called once, and because our
  // notion of a fragment doesn't really map to the Application as a whole,
  // call this function once.
  variations_service_->OnAppEnterForeground();
}

void FeatureListCreator::SetUpFieldTrials() {
#if BUILDFLAG(IS_ANDROID)
  // The FieldTrialList should have been instantiated in
  // AndroidMetricsServiceClient::Initialize().
  DCHECK(base::FieldTrialList::GetInstance());
  DCHECK(system_network_context_manager_);

  auto* metrics_client = WebLayerMetricsServiceClient::GetInstance();
  variations_service_ = variations::VariationsService::Create(
      std::make_unique<WebLayerVariationsServiceClient>(
          system_network_context_manager_),
      local_state_, metrics_client->metrics_state_manager(),
      switches::kDisableBackgroundNetworking, variations::UIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
  variations_service_->OverridePlatform(
      variations::Study::PLATFORM_ANDROID_WEBLAYER, "android_weblayer");

  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  variations_service_->SetUpFieldTrials(
      variation_ids,
      command_line->GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      content::GetSwitchDependentFeatureOverrides(*command_line),
      std::move(feature_list), &weblayer_field_trials_);
  variations::InitCrashKeys();
#else
  // TODO(weblayer-dev): Support variations on desktop.
#endif
}

}  // namespace weblayer
