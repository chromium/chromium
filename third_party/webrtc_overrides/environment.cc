// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/environment.h"

#include <memory>
#include <string_view>

#include "third_party/webrtc/api/environment/environment.h"
#include "third_party/webrtc/api/environment/environment_factory.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

webrtc::Environment WebRtcEnvironment() {
  // TODO: bugs.webrtc.org/42220378 - Inject chromium specific field trials
  // instead of relying on link-time injection of the global WebRTC field
  // trials.
  return webrtc::CreateEnvironment(CreateWebRtcTaskQueueFactory());
}
