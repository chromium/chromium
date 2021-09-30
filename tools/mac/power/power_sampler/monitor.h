// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MAC_POWER_POWER_SAMPLER_MONITOR_H_
#define TOOLS_MAC_POWER_POWER_SAMPLER_MONITOR_H_

#include <memory>
#include <vector>

#include "base/time/time.h"

namespace power_sampler {

class Sample;
class Sampler;

// Concrete monitor classes override this interface.
class Monitor {
 public:
  using Samples = std::vector<Sample>;
  using Samplers = std::vector<std::unique_ptr<Sampler>>;

  Monitor() = default;
  virtual ~Monitor() = 0;

  // TODO(siggi): Add more callouts.
  //     - Add a callout for pre-session notification with all samplers.
  //     - Add a callot for post-session.
  //     - This will allow monitors to do the output, whether it's done as you
  //       go for e.g. CSV, or all-in-one for e.g. JSON.

  // Called once before any OnSample calls are made.
  // Can be used to e.g. open a file, output a file header or other
  // one-time setup.
  virtual void OnStartSession(const Samplers& samplers) = 0;

  // Called each time a new set of |samples| has been acquired.
  // The |sample_time| is the time when the acquisition of |samples| started.
  // Returns true if the sampling session should be ended.
  virtual bool OnSample(base::TimeTicks sample_time,
                        const Samples& samples) = 0;

  // Called once after all OnSample calls have been made.
  // Can be used to e.g. close files, flush output or other one-time teardown.
  virtual void OnEndSession() = 0;
};

}  // namespace power_sampler

#endif  // TOOLS_MAC_POWER_POWER_SAMPLER_MONITOR_H_
