// Copyright 2021 The Chromium Authors
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

  for (const auto& name_and_unit : new_sampler->GetDatumNameUnits()) {
    bool inserted =
        data_columns_units_
            .emplace(DataColumnKey{new_sampler->GetName(), name_and_unit.first},
                     name_and_unit.second)
            .second;
    DCHECK(inserted);
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
    monitor->OnStartSession(data_columns_units_);

  started_ = true;
}

bool SamplingController::HasSamplers() {
  return !samplers_.empty();
}

bool SamplingController::OnSamplingEvent() {
  DCHECK(started_);

  DataRow data_row;
  const base::TimeTicks sample_time = base::TimeTicks::Now();
  for (auto& sampler : samplers_) {
    Sampler::Sample sample = sampler->GetSample(sample_time);
    for (const auto& value : sample) {
      DataColumnKey column_key{sampler->GetName(), value.first};
      DCHECK(base::Contains(data_columns_units_, column_key));
      data_row.emplace(column_key, value.second);
    }
  }

  // Notify all monitors of the new sample, and make sure we stop sampling
  // after this round if any of them want out.
  bool should_end_session = false;
  for (auto& monitor : monitors_)
    if (monitor->OnSample(sample_time, data_row))
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
