// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/battery_sampler.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"

namespace power_sampler {
namespace {

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is nullptr or it could not be converted to SInt64.
absl::optional<SInt64> GetValueAsSInt64(CFDictionaryRef description,
                                        CFStringRef key) {
  CFNumberRef number_ref =
      base::mac::GetValueFromDictionary<CFNumberRef>(description, key);

  SInt64 value;
  if (number_ref && CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value))
    return value;

  return absl::nullopt;
}

absl::optional<bool> GetValueAsBoolean(CFDictionaryRef description,
                                       CFStringRef key) {
  CFBooleanRef boolean =
      base::mac::GetValueFromDictionary<CFBooleanRef>(description, key);
  if (!boolean)
    return absl::nullopt;
  return CFBooleanGetValue(boolean);
}

}  // namespace

BatterySampler::~BatterySampler() = default;

// static
std::unique_ptr<BatterySampler> BatterySampler::Create() {
  // Retrieve the IOPMPowerSource service.
  base::mac::ScopedIOObject<io_service_t> power_source(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPMPowerSource")));
  if (power_source == IO_OBJECT_NULL)
    return nullptr;

  return CreateImpl(MaybeGetBatteryData, std::move(power_source));
}

std::string BatterySampler::GetName() {
  return kSamplerName;
}

Sampler::DatumNameUnits BatterySampler::GetDatumNameUnits() {
  DatumNameUnits ret;
  ret.emplace("external_connected", "bool");
  ret.emplace("voltage", "V");
  ret.emplace("current_capacity", "Ah");
  ret.emplace("max_capacity", "Ah");
  ret.emplace("avg_power", "W");

  return ret;
}

Sampler::Sample BatterySampler::GetSample(base::TimeTicks sample_time) {
  absl::optional<BatteryData> new_battery_data =
      maybe_get_battery_data_fn_(power_source_.get());
  if (!new_battery_data.has_value())
    return Sample();

  Sample sample;
  const BatteryData& new_data = new_battery_data.value();
  if (prev_battery_data_.has_value()) {
    // There's a previous sample to refer to. Compute the average power if
    // there's been any reported current consumption since that sample.
    // Note that the current consumption is reported in integral units of mAh,
    // and that the underlying sampling when on battery is once a minute.
    auto avg_power =
        MaybeComputeAvgPowerConsumption(sample_time - prev_battery_sample_time_,
                                        prev_battery_data_.value(), new_data);

    if (avg_power.has_value()) {
      sample.emplace("avg_power", avg_power.value());

      // The previous sample is consumed, store the new one.
      StoreBatteryData(sample_time, new_data);
    }
  }

  sample.emplace("external_connected", new_data.external_connected);
  sample.emplace("voltage", new_data.voltage_mv / 1000.0);
  sample.emplace("current_capacity", new_data.current_capacity_mah / 1000.0);
  sample.emplace("max_capacity", new_data.max_capacity_mah / 1000.0);

  // Store the battery state only if the consumed capacity is different from the
  // initial state. If the consumed capacity is identical to the initial state,
  // it would be incorrect to use it for power estimate because it's unknown for
  // how long it hasn't changed (and therefore it's unknown what time interval
  // should be used to compute the power estimate).
  if (!prev_battery_data_.has_value() &&
      (new_data.max_capacity_mah - new_data.current_capacity_mah) >
          initial_consumed_mah_) {
    // Store an initial sample.
    StoreBatteryData(sample_time, new_data);
  }

  return sample;
}

// static
absl::optional<BatterySampler::BatteryData> BatterySampler::MaybeGetBatteryData(
    io_service_t power_source) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      power_source, dict.InitializeInto(), 0, 0);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "IORegistryEntryCreateCFProperties";
    return absl::nullopt;
  }

  absl::optional<bool> external_connected =
      GetValueAsBoolean(dict, CFSTR("ExternalConnected"));
  absl::optional<SInt64> voltage_mv =
      GetValueAsSInt64(dict, CFSTR(kIOPSVoltageKey));
  absl::optional<SInt64> current_capacity_mah =
      GetValueAsSInt64(dict, CFSTR("AppleRawCurrentCapacity"));
  absl::optional<SInt64> max_capacity_mah =
      GetValueAsSInt64(dict, CFSTR("AppleRawMaxCapacity"));

  if (!external_connected.has_value() || !voltage_mv.has_value() ||
      !current_capacity_mah.has_value() || !max_capacity_mah.has_value()) {
    return absl::nullopt;
  }

  BatteryData data{.external_connected = external_connected.value(),
                   .voltage_mv = voltage_mv.value(),
                   .current_capacity_mah = current_capacity_mah.value(),
                   .max_capacity_mah = max_capacity_mah.value()};

  return data;
}

//  static
absl::optional<double> BatterySampler::MaybeComputeAvgPowerConsumption(
    base::TimeDelta duration,
    const BatteryData& prev_data,
    const BatteryData& new_data) {
  // The gauging hardware measures current consumed (or charged), but reports
  // the remaining capacity with respect to a load-dependent max capacity.
  // Here, however, we care about the delta capacity consumed rather than the
  // capacity remaining. To get to capacity consumed, we flip the capcacity
  // remaining estimates to capacity consumed and work from there. It's been
  // experimentally determined that this backs out the effects of any
  // load-dependent max capacity estimates to yield the capacity consumed.
  int64_t prev_current_consumed_mah =
      prev_data.max_capacity_mah - prev_data.current_capacity_mah;
  int64_t new_current_consumed_mah =
      new_data.max_capacity_mah - new_data.current_capacity_mah;

  // Compute the consumed capacity delta.
  int64_t delta_current_consumed_mah =
      prev_current_consumed_mah - new_current_consumed_mah;
  if (delta_current_consumed_mah == 0)
    return absl::nullopt;

  double avg_voltage_v =
      (new_data.voltage_mv + prev_data.voltage_mv) / (2.0 * 1000.0);

  constexpr double kAsPerMAh =
      static_cast<double>(base::TimeTicks::kSecondsPerHour) / 1000.0;
  double avg_current_a =
      delta_current_consumed_mah * kAsPerMAh / duration.InSecondsF();

  // This is arbitrarily defined as "power consumed" positive by flipping
  // the sign on the average current consumed. This means current stored
  // (charging) will be reported as negative power.
  return avg_voltage_v * -avg_current_a;
}

// static
std::unique_ptr<BatterySampler> BatterySampler::CreateImpl(
    MaybeGetBatteryDataFn maybe_get_battery_data_fn,
    base::mac::ScopedIOObject<io_service_t> power_source) {
  // Validate that we can work with this source.
  auto battery_data = maybe_get_battery_data_fn(power_source.get());
  if (!battery_data.has_value())
    return nullptr;

  return base::WrapUnique(new BatterySampler(
      maybe_get_battery_data_fn, std::move(power_source), *battery_data));
}

BatterySampler::BatterySampler(
    MaybeGetBatteryDataFn maybe_get_battery_data_fn,
    base::mac::ScopedIOObject<io_service_t> power_source,
    BatteryData initial_battery_data)
    : maybe_get_battery_data_fn_(maybe_get_battery_data_fn),
      power_source_(std::move(power_source)),
      initial_consumed_mah_(initial_battery_data.max_capacity_mah -
                            initial_battery_data.current_capacity_mah) {}

void BatterySampler::StoreBatteryData(base::TimeTicks sample_time,
                                      const BatteryData& battery_data) {
  prev_battery_sample_time_ = sample_time;
  prev_battery_data_ = battery_data;
}

}  // namespace power_sampler
