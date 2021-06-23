// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_
#define WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_

#include <memory>

#include "weblayer/browser/weblayer_field_trials.h"

class PrefService;

namespace variations {
class VariationsService;
}

namespace weblayer {
class SystemNetworkContextManager;

// Used by WebLayer to set up field trials based on the stored variations
// seed data. Once created this object must exist for the lifetime of the
// process as it contains the FieldTrialList that can be queried for the state
// of experiments.
class FeatureListCreator {
 public:
  explicit FeatureListCreator(PrefService* local_state);
  FeatureListCreator(const FeatureListCreator&) = delete;
  FeatureListCreator& operator=(const FeatureListCreator&) = delete;
  ~FeatureListCreator();

  // Return the single instance of FeatureListCreator. This does *not* trigger
  // creation.
  static FeatureListCreator* GetInstance();

  void SetSystemNetworkContextManager(
      SystemNetworkContextManager* system_network_context_manager);

  // Must be called after SetSharedURLLoaderFactory.
  void CreateFeatureListAndFieldTrials();

  // Called from content::BrowserMainParts::PreMainMessageLoopRun() to perform
  // initialization necessary prior to running the main message loop.
  void PerformPreMainMessageLoopStartup();

  // Calls through to the VariationService.
  void OnBrowserFragmentStarted();

  variations::VariationsService* variations_service() const {
    return variations_service_.get();
  }

 private:
  void SetUpFieldTrials();

  // Owned by BrowserProcess.
  PrefService* local_state_;

  SystemNetworkContextManager* system_network_context_manager_;  // NOT OWNED.

  std::unique_ptr<variations::VariationsService> variations_service_;

  WebLayerFieldTrials weblayer_field_trials_;

  // Set to true the first time OnBrowserFragmentStarted() is called.
  bool has_browser_fragment_started_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FEATURE_LIST_CREATOR_H_
