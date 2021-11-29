// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/smc_sampler.h"
#include <memory>

#include "base/memory/ptr_util.h"
#include "components/power_metrics/smc_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"

namespace power_sampler {

namespace {

using testing::UnorderedElementsAre;

class TestSMCReader : public power_metrics::SMCReader {
 public:
  TestSMCReader()
      : power_metrics::SMCReader(base::mac::ScopedIOObject<io_object_t>()) {}

  void set_total_power(absl::optional<double> total_power) {
    total_power_ = total_power;
  }
  void set_cpu_package_cpu_power(absl::optional<double> cpu_package_cpu_power) {
    cpu_package_cpu_power_ = cpu_package_cpu_power;
  }
  void set_cpu_package_gpu_power(absl::optional<double> cpu_package_gpu_power) {
    cpu_package_gpu_power_ = cpu_package_gpu_power;
  }
  void set_gpu0_power(absl::optional<double> gpu0_power) {
    gpu0_power_ = gpu0_power;
  }
  void set_gpu1_power(absl::optional<double> gpu1_power) {
    gpu1_power_ = gpu1_power;
  }

  // power_metrics::SMCReader:
  absl::optional<double> ReadTotalPowerW() override { return total_power_; }
  absl::optional<double> ReadCPUPackageCPUPowerW() override {
    return cpu_package_cpu_power_;
  }
  absl::optional<double> ReadCPUPackageGPUPowerW() override {
    return cpu_package_gpu_power_;
  }
  absl::optional<double> ReadGPU0PowerW() override { return gpu0_power_; }
  absl::optional<double> ReadGPU1PowerW() override { return gpu1_power_; }

 private:
  absl::optional<double> total_power_;
  absl::optional<double> cpu_package_cpu_power_;
  absl::optional<double> cpu_package_gpu_power_;
  absl::optional<double> gpu0_power_;
  absl::optional<double> gpu1_power_;
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
                                   std::make_pair("gpu1_power", "w")));
}

TEST_F(SMCSamplerTest, GetSample_AllFieldsAvailable) {
  reader_->set_total_power(1);
  reader_->set_cpu_package_cpu_power(2);
  reader_->set_cpu_package_gpu_power(3);
  reader_->set_gpu0_power(4);
  reader_->set_gpu1_power(5);

  Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
  EXPECT_THAT(sample,
              UnorderedElementsAre(std::make_pair("total_power", 1),
                                   std::make_pair("cpu_package_cpu_power", 2),
                                   std::make_pair("cpu_package_gpu_power", 3),
                                   std::make_pair("gpu0_power", 4),
                                   std::make_pair("gpu1_power", 5)));
}

TEST_F(SMCSamplerTest, GetSample_IndividualFieldNotAvailable) {
  reader_->set_total_power(1);
  reader_->set_cpu_package_cpu_power(2);
  reader_->set_cpu_package_gpu_power(3);
  reader_->set_gpu0_power(4);
  reader_->set_gpu1_power(5);

  {
    reader_->set_total_power(absl::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5)));
    reader_->set_total_power(1);
  }

  {
    reader_->set_cpu_package_cpu_power(absl::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5)));
    reader_->set_cpu_package_cpu_power(2);
  }

  {
    reader_->set_cpu_package_gpu_power(absl::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("gpu0_power", 4),
                                     std::make_pair("gpu1_power", 5)));
    reader_->set_cpu_package_gpu_power(3);
  }

  {
    reader_->set_gpu0_power(absl::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu1_power", 5)));
    reader_->set_gpu0_power(4);
  }

  {
    reader_->set_gpu1_power(absl::nullopt);
    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("total_power", 1),
                                     std::make_pair("cpu_package_cpu_power", 2),
                                     std::make_pair("cpu_package_gpu_power", 3),
                                     std::make_pair("gpu0_power", 4)));
    reader_->set_gpu1_power(5);
  }
}

}  // namespace power_sampler
