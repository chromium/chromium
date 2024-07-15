// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/smc_sampler.h"

#include <string_view>

#include "base/memory/ptr_util.h"
#include "components/power_metrics/smc_mac.h"

namespace power_sampler {

namespace {

void MaybeAddToSample(Sampler::Sample* sample,
                      std::string_view name,
                      std::optional<double> val) {
  if (val.has_value())
    sample->emplace(name, val.value());
}

}  // namespace

SMCSampler::~SMCSampler() = default;

// static
std::unique_ptr<SMCSampler> SMCSampler::Create() {
  std::unique_ptr<power_metrics::SMCReader> smc_reader =
      power_metrics::SMCReader::Create();
  if (!smc_reader)
    return nullptr;
  return base::WrapUnique(new SMCSampler(std::move(smc_reader)));
}

std::string SMCSampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits SMCSampler::GetDatumNameUnits() {
  DatumNameUnits ret{{"total_power", "w"},
                     {"cpu_package_cpu_power", "w"},
                     {"cpu_package_gpu_power", "w"},
                     {"gpu0_power", "w"},
                     {"gpu1_power", "w"},
                     {"cpu_temperature", "C"}};
  return ret;
}

Sampler::Sample SMCSampler::GetSample(base::TimeTicks sample_time) {
  Sample sample;

  MaybeAddToSample(&sample, "total_power",
                   smc_reader_->ReadKey(SMCKeyIdentifier::TotalPower));
  MaybeAddToSample(&sample, "cpu_package_cpu_power",
                   smc_reader_->ReadKey(SMCKeyIdentifier::CPUPower));
  MaybeAddToSample(&sample, "cpu_package_gpu_power",
                   smc_reader_->ReadKey(SMCKeyIdentifier::iGPUPower));
  MaybeAddToSample(&sample, "gpu0_power",
                   smc_reader_->ReadKey(SMCKeyIdentifier::GPU0Power));
  MaybeAddToSample(&sample, "gpu1_power",
                   smc_reader_->ReadKey(SMCKeyIdentifier::GPU1Power));
  MaybeAddToSample(&sample, "cpu_temperature",
                   smc_reader_->ReadKey(SMCKeyIdentifier::CPUTemperature));

  return sample;
}

SMCSampler::SMCSampler(std::unique_ptr<power_metrics::SMCReader> smc_reader)
    : smc_reader_(std::move(smc_reader)) {
  DCHECK(smc_reader_);
}

}  // namespace power_sampler
