// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLING_EVENT_SOURCE_H_
#define TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLING_EVENT_SOURCE_H_

#include "base/callback_forward.h"

namespace power_sampler {

// Invokes a callback when a Sample should be requested from all Samplers.
class SamplingEventSource {
 public:
  using SamplingEventCallback = base::RepeatingClosure;

  virtual ~SamplingEventSource() = 0;

  // Starts generating sampling events. Returns whether the operation succeeded.
  // |callback| is invoked for every sampling event.
  virtual bool Start(SamplingEventCallback callback) = 0;
};

}  // namespace power_sampler

#endif  // TOOLS_MAC_POWER_POWER_SAMPLER_SAMPLING_EVENT_SOURCE_H_
