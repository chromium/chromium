// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/field_trial.h"

namespace webrtc::field_trial {

// TODO: bugs.webrtc.org/42220378 - Remove this function when it's declaration
// is removed from WebRTC giving more confidence it would never be called.
std::string FindFullName(std::string_view trial_name) {
  // This function should never be called - in chromium all WebRTC field trials
  // should be queried through the `WebRtcFieldTrials` class.
  LOG(ERROR) << "WebRTC field trial \"" << trial_name
             << "\" is used through the free function. Such usage should be "
                "migrated away to use propagated WebRTC field trials. See "
                "https://bugs.webrtc.org/42220378";
  return base::FieldTrialList::FindFullName(trial_name);
}

}  // namespace webrtc::field_trial
