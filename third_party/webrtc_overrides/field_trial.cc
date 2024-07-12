// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/metrics/field_trial.h"

// Define webrtc::field_trial::FindFullName to provide webrtc with a field trial
// implementation.
namespace webrtc {
namespace field_trial {

std::string FindFullName(std::string_view trial_name) {
  return base::FieldTrialList::FindFullName(trial_name);
}

}  // namespace field_trial
}  // namespace webrtc
