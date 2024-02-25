// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/smc_sampler.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "components/power_metrics/smc_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"

namespace power_sampler {

namespace {

using testing::UnorderedElementsAre;

class TestSMCReader : public power_metrics::SMCReader {
 public:
  TestSMCReader()
      : power_metrics::SMCReader(base::mac::ScopedIOObject<io_object_t>()) {}

  void set_key(SMCKeyIdentifier key, std::optional<double> value) {
    keys_[key] = value;
  }

  // power_metrics::SMCReader:
  std::optional<double> ReadKey(SMCKeyIdentifier identifier) override {
    return keys_[identifier];
  }

 private:
  base::flat_map<SMCKeyIdentifier, std::optional<double>> keys_;
};

}  // namespace

class SMCSamplerTest : public testing::Test {
 public:
  SMCSamplerTest() {
    std::unique_ptr<TestSMCReader> reader = std::make_unique<TestSMCReader>();
    reader_ = reader.get();
    sampler_ = base::WrapUnique(new SMCSampler(std::move(reader)));
  }

  TestSMCReader* reader_ = nullptr;
  std::unique_ptr<SMCSampler> sampler_;
};

TEST_F(SMCSamplerTest, NameAndGetDatumNameUnits) {
  EXPECT_EQ("smc", sampler_->GetName());

  auto datum_name_units = sampler_->GetDatumNameUnits();
  EXPECT_THAT(datum_name_units,
              UnorderedElementsAre(std::make_pair("total_power", "w"),
                                   std::make_pair("cpu_package_cpu_power", "w"),
                                   std::make_pair("cpu_package_gpu_power", "w"),
                                   std::make_pair("gpu0_power", "w"),
                                   std::make_pair("gpu1_power", "w"),
                                   std::make_pair("cpu_temperature", "C")));
}

TEST_F(SMCSamplerTest, GetSample_AllFieldsAvailable) {
  reader_->set_key(SMCKeyIdentifier::TotalPower, 1);
  reader_->set_key(SMCKeyIdentifier::CPUPower, 2);
  reader_->set_key(SMCKeyIdentifier::iGPUPower, 3);
  reader_->set_key(SMCKeyIdentifier::GPU0Power, 4);
  reader_->set_key(SMCKeyIdentifier::GPU1Power, 5);
  reader_->set_key(SMCKeyIdentifier::CPUTemperature, 6);

  Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
  EXPECT_THAT(sample,
              UnorderedElementsAre(std::make_pair("total_power", 1),
                                   std::make_pair("cpu_package_cpu_power", 2),
                                   std::make_pair("cpu_package_gpu_power", 3),
                                   std::make_pair("gpu0_power", 4),
                                   std::make_pair("gpu1_power", 5),
                                   std::make_pair("cpu_temperature", 6)));
}

TEST_F(SMCSamplerTest, GetSample_IndividualFieldNotAvailable) {
  reader_->set_key(SMCKeyIdentifier::TotalPower, 1);
  reader_->set_key(SMCKeyIdentifier::CPUPower, 2);
  reader_->set_key(SMCKeyIdentifier::iGPUPower, 3);
  reader_->set_key(SMCKeyIdentifier::GPU0Power, 4);
  reader_->set_key(SMCKeyIdentifier::GPU1Power, 5);
  reader_->set_key(SMCKeyIdentifier::CPUTemperature, 6);

  {
    reader_->set_key(SMCKeyIdentifier::TotalPower, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5),
                                     std::make_pair("cpu_temperature", 6)));
    reader_->set_key(SMCKeyIdentifier::TotalPower, 1);
  }

  {
    reader_->set_key(SMCKeyIdentifier::CPUPower, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5),
                                     std::make_pair("cpu_temperature", 6)));
    reader_->set_key(SMCKeyIdentifier::CPUPower, 2);
  }

  {
    reader_->set_key(SMCKeyIdentifier::iGPUPower, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5),
                                     std::make_pair("cpu_temperature", 6)));
    reader_->set_key(SMCKeyIdentifier::iGPUPower, 3);
  }

  {
    reader_->set_key(SMCKeyIdentifier::GPU0Power, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu1_power", 5),
                                     std::make_pair("cpu_temperature", 6)));
    reader_->set_key(SMCKeyIdentifier::GPU0Power, 4);
  }

  {
    reader_->set_key(SMCKeyIdentifier::GPU1Power, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("cpu_temperature", 6)));
    reader_->set_key(SMCKeyIdentifier::GPU1Power, 5);
  }

  {
    reader_->set_key(SMCKeyIdentifier::CPUTemperature, std::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5)));
    reader_->set_key(SMCKeyIdentifier::CPUTemperature, 6);
  }
}

}  // namespace power_sampler
