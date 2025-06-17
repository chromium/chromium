// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/environment.h"

#include <memory>

#include "third_party/webrtc/api/environment/environment.h"
#include "third_party/webrtc/api/environment/environment_factory.h"
#include "third_party/webrtc_overrides/field_trial.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

webrtc::Environment WebRtcEnvironment() {
  return webrtc::CreateEnvironment(std::make_unique<WebRtcFieldTrials>(),
                                   CreateWebRtcTaskQueueFactory());
}
