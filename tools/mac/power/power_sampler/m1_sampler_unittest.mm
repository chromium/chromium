// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/m1_sampler.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "components/power_metrics/m1_sensors_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"

namespace power_sampler {

namespace {

using testing::UnorderedElementsAre;

class TestM1SensorsReader : public power_metrics::M1SensorsReader {
 public:
  TestM1SensorsReader()
      : power_metrics::M1SensorsReader(
            base::apple::ScopedCFTypeRef<IOHIDEventSystemClientRef>()) {}

  void set_temperatures(TemperaturesCelsius temperatures) {
    temperatures_ = temperatures;
  }

  // power_metrics::M1SensorsReader:
  TemperaturesCelsius ReadTemperatures() override { return temperatures_; }

 private:
  TemperaturesCelsius temperatures_;
};

}  // namespace

class M1SamplerTest : public testing::Test {
 public:
  M1SamplerTest() {
    std::unique_ptr<TestM1SensorsReader> reader =
        std::make_unique<TestM1SensorsReader>();
    reader_ = reader.get();
    sampler_ = base::WrapUnique(new M1Sampler(std::move(reader)));
  }

  TestM1SensorsReader* reader_ = nullptr;
  std::unique_ptr<M1Sampler> sampler_;
};

TEST_F(M1SamplerTest, NameAndGetDatumNameUnits) {
  EXPECT_EQ("m1", sampler_->GetName());

  auto datum_name_units = sampler_->GetDatumNameUnits();
  EXPECT_THAT(datum_name_units,
              UnorderedElementsAre(std::make_pair("p_cores_temperature", "C"),
                                   std::make_pair("e_cores_temperature", "C")));
}

TEST_F(M1SamplerTest, GetSample_AllFieldsAvailable) {
  power_metrics::M1SensorsReader::TemperaturesCelsius temperatures;
  temperatures.p_cores = 1;
  temperatures.e_cores = 2;
  reader_->set_temperatures(temperatures);

  Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
  EXPECT_THAT(sample,
              UnorderedElementsAre(std::make_pair("p_cores_temperature", 1),
                                   std::make_pair("e_cores_temperature", 2)));
}

TEST_F(M1SamplerTest, GetSample_IndividualFieldNotAvailable) {
  {
    power_metrics::M1SensorsReader::TemperaturesCelsius temperatures;
    temperatures.p_cores = std::nullopt;
    temperatures.e_cores = 2;
    reader_->set_temperatures(temperatures);

    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("e_cores_temperature", 2)));
  }

  {
    power_metrics::M1SensorsReader::TemperaturesCelsius temperatures;
    temperatures.p_cores = 1;
    temperatures.e_cores = std::nullopt;
    reader_->set_temperatures(temperatures);

    Sampler::Sample sample = sampler_->GetSample(base::TimeTicks());
    EXPECT_THAT(sample,
                UnorderedElementsAre(std::make_pair("p_cores_temperature", 1)));
  }
}

}  // namespace power_sampler
