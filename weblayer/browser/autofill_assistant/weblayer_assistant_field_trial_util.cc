// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/autofill_assistant/weblayer_assistant_field_trial_util.h"

#include "components/variations/synthetic_trials.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/weblayer_metrics_service_accessor.h"

using ::base::StringPiece;

namespace weblayer {

bool WebLayerAssistantFieldTrialUtil::RegisterSyntheticFieldTrial(
    StringPiece trial_name,
    StringPiece group_name) const {
  return WebLayerMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      WebLayerMetricsServiceClient::GetInstance()->GetMetricsService(),
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kNextLog);
}

}  // namespace weblayer
