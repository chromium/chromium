// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/field_trial.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/field_trial.h"

std::string WebRtcFieldTrials::Lookup(std::string_view trial_name) const {
  return base::FieldTrialList::FindFullName(trial_name);
}
