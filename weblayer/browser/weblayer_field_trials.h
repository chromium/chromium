// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBLAYER_FIELD_TRIALS_H_
#define WEBLAYER_BROWSER_WEBLAYER_FIELD_TRIALS_H_

#include "components/variations/platform_field_trials.h"

namespace weblayer {

// Responsible for setting up field trials specific to WebLayer. Currently all
// functions are stubs, as WebLayer has no specific field trials.
class WebLayerFieldTrials : public variations::PlatformFieldTrials {
 public:
  WebLayerFieldTrials() = default;

  WebLayerFieldTrials(const WebLayerFieldTrials&) = delete;
  WebLayerFieldTrials& operator=(const WebLayerFieldTrials&) = delete;

  ~WebLayerFieldTrials() override = default;

  // variations::PlatformFieldTrials:
  void OnVariationsSetupComplete() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBLAYER_FIELD_TRIALS_H_
