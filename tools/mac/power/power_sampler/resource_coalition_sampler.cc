// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/resource_coalition_sampler.h"

#include "base/memory/ptr_util.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "components/power_metrics/mach_time_mac.h"

namespace power_sampler {

namespace {

template <typename T>
double RatePerSecond(T quantity, base::TimeDelta duration) {
  return static_cast<double>(quantity) / duration.InSecondsF();
}

double RatePerSecondFromMachTime(int64_t mach_time,
                                 const mach_timebase_info_data_t& timebase,
                                 base::TimeDelta duration) {
  return RatePerSecond(power_metrics::MachTimeToNs(mach_time, timebase),
                       duration);
}

}  // namespace

ResourceCoalitionSampler::~ResourceCoalitionSampler() = default;

// static
std::unique_ptr<ResourceCoalitionSampler> ResourceCoalitionSampler::Create(
    base::ProcessId pid,
    base::TimeTicks start_time) {
  return Create(pid, start_time, &power_metrics::GetProcessCoalitionId,
                &power_metrics::GetCoalitionResourceUsage,
                power_metrics::GetSystemMachTimeBase());
}

std::string ResourceCoalitionSampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits ResourceCoalitionSampler::GetDatumNameUnits() {
  DatumNameUnits ret{{"tasks_started", "tasks/s"},
                     {"tasks_exited", "tasks/s"},
                     {"time_nonempty", "ns/s"},
                     {"cpu_time", "ns/s"},
                     {"interrupt_wakeups", "wakeups/s"},
                     {"platform_idle_wakeups", "wakeups/s"},
                     {"bytesread", "bytes/s"},
                     {"byteswritten", "bytes/s"},
                     {"gpu_time", "ns/s"},
                     {"cpu_time_billed_to_me", "ns/s"},
                     {"cpu_time_billed_to_others", "ns/s"},
                     {"energy", "nw"},
                     {"logical_immediate_writes", "writes/s"},
                     {"logical_deferred_writes", "writes/s"},
                     {"logical_invalidated_writes", "writes/s"},
                     {"logical_metadata_writes", "writes/s"},
                     {"logical_immediate_writes_to_external", "writes/s"},
                     {"logical_deferred_writes_to_external", "writes/s"},
                     {"logical_invalidated_writes_to_external", "writes/s"},
                     {"logical_metadata_writes_to_external", "writes/s"},
                     {"energy_billed_to_me", "nw"},
                     {"energy_billed_to_others", "nw"},
                     {"cpu_ptime", "ns/s"},
                     {"cpu_time_qos_background", "ns/s"},
                     {"cpu_time_qos_default", "ns/s"},
                     {"cpu_time_qos_legacy", "ns/s"},
                     {"cpu_time_qos_maintenance", "ns/s"},
                     {"cpu_time_qos_user_initiated", "ns/s"},
                     {"cpu_time_qos_user_interactive", "ns/s"},
                     {"cpu_time_qos_utility", "ns/s"},
                     {"cpu_instructions", "instructions/s"},
                     {"cpu_cycles", "cycles/s"},
                     {"fs_metadata_writes", "writes/s"},
                     {"pm_writes", "writes/s"},
                     {"energy_impact", "EnergyImpact/s"}};
  return ret;
}

Sampler::Sample ResourceCoalitionSampler::GetSample(
    base::TimeTicks sample_time) {
  std::unique_ptr<coalition_resource_usage> current_stats =
      get_coalition_resource_usage_fn_(coalition_id_);

  // Current stats are not available: discard previous stats so that they aren't
  // used to compute a difference in the future.
  if (!current_stats) {
    previous_time_ = base::TimeTicks();
    previous_stats_.reset();
    return Sample();
  }

  // Previous stats are not available: store the current stats so that they can
  // be used to compute a difference in the future.
  if (!previous_stats_) {
    previous_time_ = sample_time;
    previous_stats_ = std::move(current_stats);
    return Sample();
  }

  // Previous and current stats are available: compute the difference and output
  // a sample.
  coalition_resource_usage diff =
      power_metrics::GetCoalitionResourceUsageDifference(*current_stats,
                                                         *previous_stats_);
  base::TimeDelta duration = sample_time - previous_time_;
  previous_time_ = sample_time;
  previous_stats_ = std::move(current_stats);

  DCHECK_GE(duration, base::TimeDelta());
  if (duration.is_zero())
    return Sample();

  Sample sample;

  sample.emplace("tasks_started", RatePerSecond(diff.tasks_started, duration));
  sample.emplace("tasks_exited", RatePerSecond(diff.tasks_exited, duration));
  sample.emplace("time_nonempty", RatePerSecond(diff.time_nonempty, duration));
  sample.emplace("cpu_time",
                 RatePerSecondFromMachTime(diff.cpu_time, timebase_, duration));
  sample.emplace("interrupt_wakeups",
                 RatePerSecond(diff.interrupt_wakeups, duration));
  sample.emplace("platform_idle_wakeups",
                 RatePerSecond(diff.platform_idle_wakeups, duration));
  sample.emplace("bytesread", RatePerSecond(diff.bytesread, duration));
  sample.emplace("byteswritten", RatePerSecond(diff.byteswritten, duration));
  sample.emplace("gpu_time",
                 RatePerSecondFromMachTime(diff.gpu_time, timebase_, duration));
  sample.emplace("cpu_time_billed_to_me",
                 RatePerSecondFromMachTime(diff.cpu_time_billed_to_me,
                                           timebase_, duration));
  sample.emplace("cpu_time_billed_to_others",
                 RatePerSecondFromMachTime(diff.cpu_time_billed_to_others,
                                           timebase_, duration));
  sample.emplace("energy", RatePerSecond(diff.energy, duration));
  sample.emplace("logical_immediate_writes",
                 RatePerSecond(diff.logical_immediate_writes, duration));
  sample.emplace("logical_deferred_writes",
                 RatePerSecond(diff.logical_deferred_writes, duration));
  sample.emplace("logical_invalidated_writes",
                 RatePerSecond(diff.logical_invalidated_writes, duration));
  sample.emplace("logical_metadata_writes",
                 RatePerSecond(diff.logical_metadata_writes, duration));
  sample.emplace(
      "logical_immediate_writes_to_external",
      RatePerSecond(diff.logical_immediate_writes_to_external, duration));
  sample.emplace(
      "logical_deferred_writes_to_external",
      RatePerSecond(diff.logical_deferred_writes_to_external, duration));
  sample.emplace(
      "logical_invalidated_writes_to_external",
      RatePerSecond(diff.logical_invalidated_writes_to_external, duration));
  sample.emplace(
      "logical_metadata_writes_to_external",
      RatePerSecond(diff.logical_metadata_writes_to_external, duration));
  sample.emplace("energy_billed_to_me",
                 RatePerSecond(diff.energy_billed_to_me, duration));
  sample.emplace("energy_billed_to_others",
                 RatePerSecond(diff.energy_billed_to_others, duration));
  sample.emplace("cpu_ptime", RatePerSecondFromMachTime(diff.cpu_ptime,
                                                        timebase_, duration));
  sample.emplace(
      "cpu_time_qos_background",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_BACKGROUND],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_default",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_DEFAULT],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_legacy",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_LEGACY],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_maintenance",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_MAINTENANCE],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_user_initiated",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_USER_INITIATED],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_user_interactive",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE],
                                timebase_, duration));
  sample.emplace(
      "cpu_time_qos_utility",
      RatePerSecondFromMachTime(diff.cpu_time_eqos[THREAD_QOS_UTILITY],
                                timebase_, duration));
  sample.emplace("cpu_instructions",
                 RatePerSecond(diff.cpu_instructions, duration));
  sample.emplace("cpu_cycles", RatePerSecond(diff.cpu_cycles, duration));
  sample.emplace("fs_metadata_writes",
                 RatePerSecond(diff.fs_metadata_writes, duration));
  sample.emplace("pm_writes", RatePerSecond(diff.pm_writes, duration));

  if (energy_impact_coefficients_.has_value()) {
    sample.emplace(
        "energy_impact",
        RatePerSecond(power_metrics::ComputeEnergyImpactForResourceUsage(
                          diff, energy_impact_coefficients_.value(), timebase_),
                      duration));
  }

  return sample;
}

// static
std::unique_ptr<ResourceCoalitionSampler> ResourceCoalitionSampler::Create(
    base::ProcessId pid,
    base::TimeTicks now,
    GetProcessCoalitionIdFn get_process_coalition_id_fn,
    GetCoalitionResourceUsageFn get_coalition_resource_usage_fn,
    mach_timebase_info_data_t timebase) {
  std::optional<uint64_t> coalition_id = get_process_coalition_id_fn(pid);
  if (!coalition_id.has_value()) {
    return nullptr;
  }

  return base::WrapUnique(new ResourceCoalitionSampler(
      coalition_id.value(), now, get_coalition_resource_usage_fn, timebase));
}

ResourceCoalitionSampler::ResourceCoalitionSampler(
    uint64_t coalition_id,
    base::TimeTicks now,
    GetCoalitionResourceUsageFn get_coalition_resource_usage_fn,
    mach_timebase_info_data_t timebase)
    : coalition_id_(coalition_id),
      get_coalition_resource_usage_fn_(get_coalition_resource_usage_fn),
      timebase_(timebase),
      energy_impact_coefficients_(
          power_metrics::ReadCoefficientsForCurrentMachineOrDefault()),
      previous_time_(now),
      previous_stats_(get_coalition_resource_usage_fn_(coalition_id_)) {}

}  // namespace power_sampler
