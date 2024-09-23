// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/m1_sampler.h"

#include <string_view>

#include "base/memory/ptr_util.h"
#include "components/power_metrics/m1_sensors_mac.h"

namespace power_sampler {

namespace {

void MaybeAddToSample(Sampler::Sample* sample,
                      std::string_view name,
                      std::optional<double> val) {
  if (val.has_value())
    sample->emplace(name, val.value());
}

}  // namespace

M1Sampler::~M1Sampler() = default;

// static
std::unique_ptr<M1Sampler> M1Sampler::Create() {
  std::unique_ptr<power_metrics::M1SensorsReader> reader =
      power_metrics::M1SensorsReader::Create();
  if (!reader)
    return nullptr;
  return base::WrapUnique(new M1Sampler(std::move(reader)));
}

std::string M1Sampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits M1Sampler::GetDatumNameUnits() {
  DatumNameUnits ret{{"p_cores_temperature", "C"},
                     {"e_cores_temperature", "C"}};
  return ret;
}

Sampler::Sample M1Sampler::GetSample(base::TimeTicks sample_time) {
  Sample sample;
  power_metrics::M1SensorsReader::TemperaturesCelsius temperatures =
      reader_->ReadTemperatures();

  MaybeAddToSample(&sample, "p_cores_temperature", temperatures.p_cores);
  MaybeAddToSample(&sample, "e_cores_temperature", temperatures.e_cores);

  return sample;
}

M1Sampler::M1Sampler(std::unique_ptr<power_metrics::M1SensorsReader> reader)
    : reader_(std::move(reader)) {
  DCHECK(reader_);
}

}  // namespace power_sampler
