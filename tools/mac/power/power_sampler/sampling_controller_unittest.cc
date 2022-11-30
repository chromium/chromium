// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/sampling_controller.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/mac/power/power_sampler/monitor.h"
#include "tools/mac/power/power_sampler/sampler.h"

namespace power_sampler {

namespace {

using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

class TestSampler : public Sampler {
 public:
  explicit TestSampler(const char* name, double sample = 0.0)
      : name_(name), sample_(sample) {}

  std::string GetName() override { return name_; }

  DatumNameUnits GetDatumNameUnits() override {
    DatumNameUnits datum_name_units;
    datum_name_units.insert(std::make_pair(name_, name_));
    return datum_name_units;
  }

  Sample GetSample(base::TimeTicks sample_time) override {
    Sample sample;
    sample.emplace(name_, sample_);
    return sample;
  }

 private:
  const std::string name_;
  const double sample_;
};

class LenientMockMonitor : public Monitor {
 public:
  LenientMockMonitor() = default;
  ~LenientMockMonitor() override = default;

  MOCK_METHOD(void,
              OnStartSession,
              ((const base::flat_map<DataColumnKey, std::string>&)));
  MOCK_METHOD(bool,
              OnSample,
              (base::TimeTicks sample_time, const DataRow& data_row));
  MOCK_METHOD(void, OnEndSession, ());
};
using MockMonitor = StrictMock<LenientMockMonitor>;

}  // namespace

TEST(SamplingControllerTest, AddSampler) {
  SamplingController controller;
  EXPECT_TRUE(controller.AddSampler(std::make_unique<TestSampler>("foo")));
  EXPECT_TRUE(controller.AddSampler(std::make_unique<TestSampler>("bar")));
  EXPECT_FALSE(controller.AddSampler(std::make_unique<TestSampler>("bar")));
}

TEST(SamplingControllerTest, CallsSamplersAndMonitors) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SamplingController controller;
  EXPECT_TRUE(controller.AddSampler(std::make_unique<TestSampler>("foo", 1.0)));
  EXPECT_TRUE(controller.AddSampler(std::make_unique<TestSampler>("bar", 2.0)));

  controller.StartSession();
  // No monitors to end the session.
  EXPECT_FALSE(controller.OnSamplingEvent());
  controller.EndSession();

  auto mock_monitor = std::make_unique<MockMonitor>();
  MockMonitor* monitor = mock_monitor.get();
  controller.AddMonitor(std::move(mock_monitor));

  EXPECT_CALL(*monitor, OnStartSession(_));
  controller.StartSession();

  base::TimeTicks first_now = base::TimeTicks::Now();
  DataRow last_seen_data_row;
  EXPECT_CALL(*monitor, OnSample(first_now, _))
      .WillOnce(DoAll(SaveArg<1>(&last_seen_data_row), Return(false)));
  EXPECT_FALSE(controller.OnSamplingEvent());

  EXPECT_THAT(last_seen_data_row,
              ElementsAre(std::make_pair(DataColumnKey{"bar", "bar"}, 2.0),
                          std::make_pair(DataColumnKey{"foo", "foo"}, 1.0)));

  last_seen_data_row.clear();

  task_environment.FastForwardBy(base::Milliseconds(1500));
  base::TimeTicks second_now = base::TimeTicks::Now();
  // Terminate the sampling session on the next sample.
  EXPECT_CALL(*monitor, OnSample(second_now, _))
      .WillOnce(DoAll(SaveArg<1>(&last_seen_data_row), Return(true)));
  EXPECT_TRUE(controller.OnSamplingEvent());
  // We still expect the same samples.
  EXPECT_THAT(last_seen_data_row,
              ElementsAre(std::make_pair(DataColumnKey{"bar", "bar"}, 2.0),
                          std::make_pair(DataColumnKey{"foo", "foo"}, 1.0)));

  EXPECT_CALL(*monitor, OnEndSession());
  controller.EndSession();
}

}  // namespace power_sampler
