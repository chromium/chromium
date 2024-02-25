// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/resource_coalition_sampler.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/process/process_handle.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "components/power_metrics/mach_time_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {

namespace {

using testing::UnorderedElementsAre;

constexpr base::ProcessId kTestPid = 42;
constexpr base::ProcessId kTestCoalitionId = 123;
constexpr mach_timebase_info_data_t kIntelTimebase = {1, 1};
mach_timebase_info_data_t kM1Timebase = {125, 3};

constexpr power_metrics::EnergyImpactCoefficients kTestEnergyImpactCoefficients{
    .kcpu_wakeups = base::Microseconds(1000).InSecondsF(),
    .kqos_default = 3000,
    .kqos_background = 6000,
    .kqos_utility = 9000,
    .kqos_legacy = 12000,
    .kqos_user_initiated = 15000,
    .kqos_user_interactive = 18000,
    .kgpu_time = 21000,
};

coalition_resource_usage GetTestCoalitionResourceUsage(uint32_t multiplier) {
  coalition_resource_usage ret{
      .tasks_started = 1 * multiplier,
      .tasks_exited = 2 * multiplier,
      .time_nonempty = 3 * multiplier,
      .cpu_time = 4 * multiplier,
      .interrupt_wakeups = 5 * multiplier,
      .platform_idle_wakeups = 6 * multiplier,
      .bytesread = 7 * multiplier,
      .byteswritten = 8 * multiplier,
      .gpu_time = 9 * multiplier,
      .cpu_time_billed_to_me = 10 * multiplier,
      .cpu_time_billed_to_others = 11 * multiplier,
      .energy = 12 * multiplier,
      .logical_immediate_writes = 13 * multiplier,
      .logical_deferred_writes = 14 * multiplier,
      .logical_invalidated_writes = 15 * multiplier,
      .logical_metadata_writes = 16 * multiplier,
      .logical_immediate_writes_to_external = 17 * multiplier,
      .logical_deferred_writes_to_external = 18 * multiplier,
      .logical_invalidated_writes_to_external = 19 * multiplier,
      .logical_metadata_writes_to_external = 20 * multiplier,
      .energy_billed_to_me = 21 * multiplier,
      .energy_billed_to_others = 22 * multiplier,
      .cpu_ptime = 23 * multiplier,
      .cpu_time_eqos_len = COALITION_NUM_THREAD_QOS_TYPES,
      .cpu_instructions = 31 * multiplier,
      .cpu_cycles = 32 * multiplier,
      .fs_metadata_writes = 33 * multiplier,
      .pm_writes = 34 * multiplier,
  };

  ret.cpu_time_eqos[THREAD_QOS_DEFAULT] = 24 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_MAINTENANCE] = 25 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_BACKGROUND] = 26 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_UTILITY] = 27 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_LEGACY] = 28 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_USER_INITIATED] = 29 * multiplier;
  ret.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE] = 30 * multiplier;

  return ret;
}

double NsToM1Timebase(int64_t ns) {
  return static_cast<double>(ns * kM1Timebase.numer) / kM1Timebase.denom;
}

}  // namespace

class ResourceCoalitionSamplerTest : public testing::Test {
 protected:
  void SetUp() override {
    set_expected_process_id(-1);
    set_coalition_id(std::nullopt);
    set_resource_usage(std::nullopt);
  }

  static void set_expected_process_id(base::ProcessId expected_process_id) {
    expected_process_id_ = expected_process_id;
  }

  static void set_coalition_id(std::optional<uint64_t> coalition_id) {
    coalition_id_ = coalition_id;
  }

  static void set_resource_usage(
      std::optional<coalition_resource_usage> resource_usage) {
    resource_usage_ = resource_usage;
  }

  // The gmock *ElementsAre* matchers are too exacting for the double values
  // in our samples, but this poor man's substitute will do for our needs.
  template <size_t N>
  void ExpectSampleMatchesArray(
      const Sampler::Sample& sample,
      const std::pair<std::string, double> (&datums)[N]) {
    EXPECT_EQ(N, sample.size());
    for (size_t i = 0; i < N; ++i) {
      const auto& name = datums[i].first;
      const double value = datums[i].second;

      auto it = sample.find(name);
      EXPECT_TRUE(it != sample.end()) << " for " << name;
      if (it != sample.end())
        EXPECT_DOUBLE_EQ(it->second, value) << " for " << name;
    }
  }

  std::unique_ptr<ResourceCoalitionSampler> CreateSampler(
      base::ProcessId pid,
      base::TimeTicks now,
      mach_timebase_info_data_t timebase,
      std::optional<power_metrics::EnergyImpactCoefficients>
          energy_impact_coefficients = kTestEnergyImpactCoefficients) {
    std::unique_ptr<ResourceCoalitionSampler> sampler =
        ResourceCoalitionSampler::Create(pid, now, &GetStaticProcessCoalitionId,
                                         &GetStaticCoalitionResourceUsage,
                                         timebase);
    if (sampler)
      sampler->energy_impact_coefficients_ = energy_impact_coefficients;
    return sampler;
  }

 private:
  static std::optional<uint64_t> GetStaticProcessCoalitionId(
      base::ProcessId pid) {
    EXPECT_EQ(pid, expected_process_id_);
    return coalition_id_;
  }

  static std::unique_ptr<coalition_resource_usage>
  GetStaticCoalitionResourceUsage(int64_t coalition_id) {
    EXPECT_EQ(coalition_id, coalition_id_);
    if (!resource_usage_.has_value())
      return nullptr;
    return std::make_unique<coalition_resource_usage>(resource_usage_.value());
  }

  static base::ProcessId expected_process_id_;
  static std::optional<int64_t> coalition_id_;
  static std::optional<coalition_resource_usage> resource_usage_;
};

// static
base::ProcessId ResourceCoalitionSamplerTest::expected_process_id_ = -1;
std::optional<int64_t> ResourceCoalitionSamplerTest::coalition_id_;
std::optional<coalition_resource_usage>
    ResourceCoalitionSamplerTest::resource_usage_;

TEST_F(ResourceCoalitionSamplerTest, CreateFailsWhenNoCoalitionId) {
  set_expected_process_id(kTestPid);
  EXPECT_EQ(nullptr,
            CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase));
}

TEST_F(ResourceCoalitionSamplerTest, CreateSucceedsWithCoalitonId) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  EXPECT_NE(nullptr,
            CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase));
}

TEST_F(ResourceCoalitionSamplerTest, NameAndGetDatumNameUnits) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  std::unique_ptr<ResourceCoalitionSampler> sampler(
      CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase));
  ASSERT_NE(nullptr, sampler);

  EXPECT_EQ("resource_coalition", sampler->GetName());

  auto datum_name_units = sampler->GetDatumNameUnits();
  EXPECT_THAT(
      datum_name_units,
      UnorderedElementsAre(
          std::make_pair("tasks_started", "tasks/s"),
          std::make_pair("tasks_exited", "tasks/s"),
          std::make_pair("time_nonempty", "ns/s"),
          std::make_pair("cpu_time", "ns/s"),
          std::make_pair("interrupt_wakeups", "wakeups/s"),
          std::make_pair("platform_idle_wakeups", "wakeups/s"),
          std::make_pair("bytesread", "bytes/s"),
          std::make_pair("byteswritten", "bytes/s"),
          std::make_pair("gpu_time", "ns/s"),
          std::make_pair("cpu_time_billed_to_me", "ns/s"),
          std::make_pair("cpu_time_billed_to_others", "ns/s"),
          std::make_pair("energy", "nw"),
          std::make_pair("logical_immediate_writes", "writes/s"),
          std::make_pair("logical_deferred_writes", "writes/s"),
          std::make_pair("logical_invalidated_writes", "writes/s"),
          std::make_pair("logical_metadata_writes", "writes/s"),
          std::make_pair("logical_immediate_writes_to_external", "writes/s"),
          std::make_pair("logical_deferred_writes_to_external", "writes/s"),
          std::make_pair("logical_invalidated_writes_to_external", "writes/s"),
          std::make_pair("logical_metadata_writes_to_external", "writes/s"),
          std::make_pair("energy_billed_to_me", "nw"),
          std::make_pair("energy_billed_to_others", "nw"),
          std::make_pair("cpu_ptime", "ns/s"),
          std::make_pair("cpu_time_qos_default", "ns/s"),
          std::make_pair("cpu_time_qos_background", "ns/s"),
          std::make_pair("cpu_time_qos_utility", "ns/s"),
          std::make_pair("cpu_time_qos_legacy", "ns/s"),
          std::make_pair("cpu_time_qos_maintenance", "ns/s"),
          std::make_pair("cpu_time_qos_user_initiated", "ns/s"),
          std::make_pair("cpu_time_qos_user_interactive", "ns/s"),
          std::make_pair("cpu_instructions", "instructions/s"),
          std::make_pair("cpu_cycles", "cycles/s"),
          std::make_pair("fs_metadata_writes", "writes/s"),
          std::make_pair("pm_writes", "writes/s"),
          std::make_pair("energy_impact", "EnergyImpact/s")));
}

TEST_F(ResourceCoalitionSamplerTest, GetSample_Available_IntelTimebase) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/1 * base::Time::kSecondsPerMinute));

  std::unique_ptr<ResourceCoalitionSampler> sampler(
      CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase));

  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/2 * base::Time::kSecondsPerMinute));
  Sampler::Sample sample =
      sampler->GetSample(base::TimeTicks() + base::Minutes(1));
  ExpectSampleMatchesArray(
      sample, {std::make_pair("tasks_started", 1),
               std::make_pair("tasks_exited", 2),
               std::make_pair("time_nonempty", 3),
               std::make_pair("cpu_time", 4),
               std::make_pair("interrupt_wakeups", 5),
               std::make_pair("platform_idle_wakeups", 6),
               std::make_pair("bytesread", 7),
               std::make_pair("byteswritten", 8),
               std::make_pair("gpu_time", 9),
               std::make_pair("cpu_time_billed_to_me", 10),
               std::make_pair("cpu_time_billed_to_others", 11),
               std::make_pair("energy", 12),
               std::make_pair("logical_immediate_writes", 13),
               std::make_pair("logical_deferred_writes", 14),
               std::make_pair("logical_invalidated_writes", 15),
               std::make_pair("logical_metadata_writes", 16),
               std::make_pair("logical_immediate_writes_to_external", 17),
               std::make_pair("logical_deferred_writes_to_external", 18),
               std::make_pair("logical_invalidated_writes_to_external", 19),
               std::make_pair("logical_metadata_writes_to_external", 20),
               std::make_pair("energy_billed_to_me", 21),
               std::make_pair("energy_billed_to_others", 22),
               std::make_pair("cpu_ptime", 23),
               std::make_pair("cpu_time_qos_default", 24),
               std::make_pair("cpu_time_qos_maintenance", 25),
               std::make_pair("cpu_time_qos_background", 26),
               std::make_pair("cpu_time_qos_utility", 27),
               std::make_pair("cpu_time_qos_legacy", 28),
               std::make_pair("cpu_time_qos_user_initiated", 29),
               std::make_pair("cpu_time_qos_user_interactive", 30),
               std::make_pair("cpu_instructions", 31),
               std::make_pair("cpu_cycles", 32),
               std::make_pair("fs_metadata_writes", 33),
               std::make_pair("pm_writes", 34),
               std::make_pair("energy_impact", 0.7971)});

  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/4 * base::Time::kSecondsPerMinute));
  sample = sampler->GetSample(base::TimeTicks() + base::Minutes(2));
  ExpectSampleMatchesArray(
      sample, {std::make_pair("tasks_started", 2),
               std::make_pair("tasks_exited", 4),
               std::make_pair("time_nonempty", 6),
               std::make_pair("cpu_time", 8),
               std::make_pair("interrupt_wakeups", 10),
               std::make_pair("platform_idle_wakeups", 12),
               std::make_pair("bytesread", 14),
               std::make_pair("byteswritten", 16),
               std::make_pair("gpu_time", 18),
               std::make_pair("cpu_time_billed_to_me", 20),
               std::make_pair("cpu_time_billed_to_others", 22),
               std::make_pair("energy", 24),
               std::make_pair("logical_immediate_writes", 26),
               std::make_pair("logical_deferred_writes", 28),
               std::make_pair("logical_invalidated_writes", 30),
               std::make_pair("logical_metadata_writes", 32),
               std::make_pair("logical_immediate_writes_to_external", 34),
               std::make_pair("logical_deferred_writes_to_external", 36),
               std::make_pair("logical_invalidated_writes_to_external", 38),
               std::make_pair("logical_metadata_writes_to_external", 40),
               std::make_pair("energy_billed_to_me", 42),
               std::make_pair("energy_billed_to_others", 44),
               std::make_pair("cpu_ptime", 46),
               std::make_pair("cpu_time_qos_default", 48),
               std::make_pair("cpu_time_qos_maintenance", 50),
               std::make_pair("cpu_time_qos_background", 52),
               std::make_pair("cpu_time_qos_utility", 54),
               std::make_pair("cpu_time_qos_legacy", 56),
               std::make_pair("cpu_time_qos_user_initiated", 58),
               std::make_pair("cpu_time_qos_user_interactive", 60),
               std::make_pair("cpu_instructions", 62),
               std::make_pair("cpu_cycles", 64),
               std::make_pair("fs_metadata_writes", 66),
               std::make_pair("pm_writes", 68),
               std::make_pair("energy_impact", 1.5942)});
}

TEST_F(ResourceCoalitionSamplerTest, GetSample_Available_M1Timebase) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/1 * base::Time::kSecondsPerMinute));

  std::unique_ptr<ResourceCoalitionSampler> sampler(
      CreateSampler(kTestPid, base::TimeTicks(), kM1Timebase));

  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/2 * base::Time::kSecondsPerMinute));
  Sampler::Sample sample =
      sampler->GetSample(base::TimeTicks() + base::Minutes(1));
  ExpectSampleMatchesArray(
      sample,
      {std::make_pair("tasks_started", 1),
       std::make_pair("tasks_exited", 2),
       std::make_pair("time_nonempty", 3),
       std::make_pair("cpu_time", NsToM1Timebase(4)),
       std::make_pair("interrupt_wakeups", 5),
       std::make_pair("platform_idle_wakeups", 6),
       std::make_pair("bytesread", 7),
       std::make_pair("byteswritten", 8),
       std::make_pair("gpu_time", NsToM1Timebase(9)),
       std::make_pair("cpu_time_billed_to_me", NsToM1Timebase(10)),
       std::make_pair("cpu_time_billed_to_others", NsToM1Timebase(11)),
       std::make_pair("energy", 12),
       std::make_pair("logical_immediate_writes", 13),
       std::make_pair("logical_deferred_writes", 14),
       std::make_pair("logical_invalidated_writes", 15),
       std::make_pair("logical_metadata_writes", 16),
       std::make_pair("logical_immediate_writes_to_external", 17),
       std::make_pair("logical_deferred_writes_to_external", 18),
       std::make_pair("logical_invalidated_writes_to_external", 19),
       std::make_pair("logical_metadata_writes_to_external", 20),
       std::make_pair("energy_billed_to_me", 21),
       std::make_pair("energy_billed_to_others", 22),
       std::make_pair("cpu_ptime", NsToM1Timebase(23)),
       std::make_pair("cpu_time_qos_default", NsToM1Timebase(24)),
       std::make_pair("cpu_time_qos_maintenance", NsToM1Timebase(25)),
       std::make_pair("cpu_time_qos_background", NsToM1Timebase(26)),
       std::make_pair("cpu_time_qos_utility", NsToM1Timebase(27)),
       std::make_pair("cpu_time_qos_legacy", NsToM1Timebase(28)),
       std::make_pair("cpu_time_qos_user_initiated", NsToM1Timebase(29)),
       std::make_pair("cpu_time_qos_user_interactive", NsToM1Timebase(30)),
       std::make_pair("cpu_instructions", 31),
       std::make_pair("cpu_cycles", 32),
       std::make_pair("fs_metadata_writes", 33),
       std::make_pair("pm_writes", 34),
       std::make_pair("energy_impact", 8.8125)});

  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/4 * base::Time::kSecondsPerMinute));
  sample = sampler->GetSample(base::TimeTicks() + base::Minutes(2));
  ExpectSampleMatchesArray(
      sample,
      {std::make_pair("tasks_started", 2),
       std::make_pair("tasks_exited", 4),
       std::make_pair("time_nonempty", 6),
       std::make_pair("cpu_time", NsToM1Timebase(8)),
       std::make_pair("interrupt_wakeups", 10),
       std::make_pair("platform_idle_wakeups", 12),
       std::make_pair("bytesread", 14),
       std::make_pair("byteswritten", 16),
       std::make_pair("gpu_time", NsToM1Timebase(18)),
       std::make_pair("cpu_time_billed_to_me", NsToM1Timebase(20)),
       std::make_pair("cpu_time_billed_to_others", NsToM1Timebase(22)),
       std::make_pair("energy", 24),
       std::make_pair("logical_immediate_writes", 26),
       std::make_pair("logical_deferred_writes", 28),
       std::make_pair("logical_invalidated_writes", 30),
       std::make_pair("logical_metadata_writes", 32),
       std::make_pair("logical_immediate_writes_to_external", 34),
       std::make_pair("logical_deferred_writes_to_external", 36),
       std::make_pair("logical_invalidated_writes_to_external", 38),
       std::make_pair("logical_metadata_writes_to_external", 40),
       std::make_pair("energy_billed_to_me", 42),
       std::make_pair("energy_billed_to_others", 44),
       std::make_pair("cpu_ptime", NsToM1Timebase(46)),
       std::make_pair("cpu_time_qos_default", NsToM1Timebase(48)),
       std::make_pair("cpu_time_qos_maintenance", NsToM1Timebase(50)),
       std::make_pair("cpu_time_qos_background", NsToM1Timebase(52)),
       std::make_pair("cpu_time_qos_utility", NsToM1Timebase(54)),
       std::make_pair("cpu_time_qos_legacy", NsToM1Timebase(56)),
       std::make_pair("cpu_time_qos_user_initiated", NsToM1Timebase(58)),
       std::make_pair("cpu_time_qos_user_interactive", NsToM1Timebase(60)),
       std::make_pair("cpu_instructions", 62),
       std::make_pair("cpu_cycles", 64),
       std::make_pair("fs_metadata_writes", 66),
       std::make_pair("pm_writes", 68),
       std::make_pair("energy_impact", 17.625)});
}

TEST_F(ResourceCoalitionSamplerTest,
       GetSample_NoEnergyImpactWithoutCoefficients) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/1 * base::Time::kSecondsPerMinute));

  std::unique_ptr<ResourceCoalitionSampler> sampler(
      CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase,
                    /* energy_impact_coefficients=*/std::nullopt));

  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/2 * base::Time::kSecondsPerMinute));
  Sampler::Sample sample =
      sampler->GetSample(base::TimeTicks() + base::Minutes(1));
  EXPECT_FALSE(base::Contains(sample, "energy_impact"));
}

TEST_F(ResourceCoalitionSamplerTest, GetSample_NotAvailable) {
  set_expected_process_id(kTestPid);
  set_coalition_id(kTestCoalitionId);
  set_resource_usage(GetTestCoalitionResourceUsage(/* multiplier=*/1));

  std::unique_ptr<ResourceCoalitionSampler> sampler(
      CreateSampler(kTestPid, base::TimeTicks(), kIntelTimebase));

  // Previous `coalition_resource_usage` is available but not the current one.
  set_resource_usage(std::nullopt);
  Sampler::Sample sample =
      sampler->GetSample(base::TimeTicks() + base::Minutes(1));
  EXPECT_TRUE(sample.empty());

  // Current `coalition_resource_usage` is available but not the previous one.
  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/2 * base::Time::kSecondsPerMinute));
  sample = sampler->GetSample(base::TimeTicks() + base::Minutes(2));
  EXPECT_TRUE(sample.empty());

  // Both current and previous `coalition_resource_usage` are available.
  set_resource_usage(GetTestCoalitionResourceUsage(
      /* multiplier=*/3 * base::Time::kSecondsPerMinute));
  sample = sampler->GetSample(base::TimeTicks() + base::Minutes(3));
  ExpectSampleMatchesArray(
      sample, {std::make_pair("tasks_started", 1),
               std::make_pair("tasks_exited", 2),
               std::make_pair("time_nonempty", 3),
               std::make_pair("cpu_time", 4),
               std::make_pair("interrupt_wakeups", 5),
               std::make_pair("platform_idle_wakeups", 6),
               std::make_pair("bytesread", 7),
               std::make_pair("byteswritten", 8),
               std::make_pair("gpu_time", 9),
               std::make_pair("cpu_time_billed_to_me", 10),
               std::make_pair("cpu_time_billed_to_others", 11),
               std::make_pair("energy", 12),
               std::make_pair("logical_immediate_writes", 13),
               std::make_pair("logical_deferred_writes", 14),
               std::make_pair("logical_invalidated_writes", 15),
               std::make_pair("logical_metadata_writes", 16),
               std::make_pair("logical_immediate_writes_to_external", 17),
               std::make_pair("logical_deferred_writes_to_external", 18),
               std::make_pair("logical_invalidated_writes_to_external", 19),
               std::make_pair("logical_metadata_writes_to_external", 20),
               std::make_pair("energy_billed_to_me", 21),
               std::make_pair("energy_billed_to_others", 22),
               std::make_pair("cpu_ptime", 23),
               std::make_pair("cpu_time_qos_default", 24),
               std::make_pair("cpu_time_qos_maintenance", 25),
               std::make_pair("cpu_time_qos_background", 26),
               std::make_pair("cpu_time_qos_utility", 27),
               std::make_pair("cpu_time_qos_legacy", 28),
               std::make_pair("cpu_time_qos_user_initiated", 29),
               std::make_pair("cpu_time_qos_user_interactive", 30),
               std::make_pair("cpu_instructions", 31),
               std::make_pair("cpu_cycles", 32),
               std::make_pair("fs_metadata_writes", 33),
               std::make_pair("pm_writes", 34),
               std::make_pair("energy_impact", 0.7971)});
}

}  // namespace power_sampler
