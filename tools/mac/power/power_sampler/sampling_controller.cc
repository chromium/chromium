// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/sampling_controller.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "tools/mac/power/power_sampler/monitor.h"
#include "tools/mac/power/power_sampler/sampler.h"

namespace power_sampler {

SamplingController::SamplingController() = default;
SamplingController::~SamplingController() {
  // Stop the session before destruction for best results.
  DCHECK(!started_);
}

bool SamplingController::AddSampler(std::unique_ptr<Sampler> new_sampler) {
  DCHECK(!started_);

  for (const auto& sampler : samplers_) {
    if (sampler->GetName() == new_sampler->GetName())
      return false;
  }

  samplers_.push_back(std::move(new_sampler));
  return true;
}

void SamplingController::AddMonitor(std::unique_ptr<Monitor> monitor) {
  DCHECK(!started_);
  monitors_.push_back(std::move(monitor));
}

void SamplingController::StartSession() {
  DCHECK(!started_);

  for (auto& monitor : monitors_)
    monitor->OnStartSession(samplers_);

  started_ = true;
}

bool SamplingController::OnSamplingEvent() {
  DCHECK(started_);

  std::vector<Sample> samples;
  const base::TimeTicks sample_time = base::TimeTicks::Now();
  for (auto& sampler : samplers_) {
    Sample sample = sampler->GetSample(sample_time);
    DCHECK_EQ(sample.sampler_name(), sampler->GetName());
    // TODO(siggi): Verify that the samplers return only the datums they
    //      declare (in debug).
    samples.push_back(std::move(sample));
  }

  // Notify all monitors of the new sample, and make sure we stop sampling
  // after this round if any of them want out.
  bool should_end_session = false;
  for (auto& monitor : monitors_)
    if (monitor->OnSample(sample_time, samples))
      should_end_session = true;

  return should_end_session;
}

void SamplingController::EndSession() {
  DCHECK(started_);

  for (auto& monitor : monitors_)
    monitor->OnEndSession();

  started_ = false;
}

}  // namespace power_sampler
