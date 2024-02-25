// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/battery_sampler.h"

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <cstdint>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"

namespace power_sampler {
namespace {

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is nullptr or it could not be converted to SInt64.
std::optional<SInt64> GetValueAsSInt64(CFDictionaryRef description,
                                       CFStringRef key) {
  CFNumberRef number_ref =
      base::apple::GetValueFromDictionary<CFNumberRef>(description, key);

  SInt64 value;
  if (number_ref && CFNumberGetValue(number_ref, kCFNumberSInt64Type, &value))
    return value;

  return std::nullopt;
}

std::optional<bool> GetValueAsBoolean(CFDictionaryRef description,
                                      CFStringRef key) {
  CFBooleanRef boolean =
      base::apple::GetValueFromDictionary<CFBooleanRef>(description, key);
  if (!boolean)
    return std::nullopt;
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
  if (!power_source) {
    return nullptr;
  }

  auto get_seconds_since_epoch_fn = []() -> int64_t {
    return (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  };

  return CreateImpl(MaybeGetBatteryData, get_seconds_since_epoch_fn,
                    std::move(power_source));
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
  // https://en.wikipedia.org/wiki/Power_(physics)
  ret.emplace("avg_power", "W");
  // https://en.wikipedia.org/wiki/Electric_charge
  ret.emplace("electric_charge_delta", "mAh");
  ret.emplace("sample_age", "s");

  return ret;
}

Sampler::Sample BatterySampler::GetSample(base::TimeTicks sample_time) {
  std::optional<BatteryData> new_battery_data =
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
    auto avg_consumption =
        MaybeComputeAvgConsumption(sample_time - prev_battery_sample_time_,
                                   prev_battery_data_.value(), new_data);

    if (avg_consumption.has_value()) {
      sample.emplace("avg_power", avg_consumption->watts);
      sample.emplace("electric_charge_delta", avg_consumption->mah);

      // The previous sample is consumed, store the new one.
      StoreBatteryData(sample_time, new_data);
    }
  }

  sample.emplace("external_connected", new_data.external_connected);
  sample.emplace("voltage", new_data.voltage_mv / 1000.0);
  sample.emplace("current_capacity", new_data.current_capacity_mah / 1000.0);
  sample.emplace("max_capacity", new_data.max_capacity_mah / 1000.0);
  sample.emplace("sample_age", get_seconds_since_epoch_fn_() -
                                   new_data.update_time_seconds_since_epoch);

  if (!prev_battery_data_.has_value()) {
    // Store an initial sample.
    StoreBatteryData(sample_time, new_data);
  }

  return sample;
}

// static
std::optional<BatterySampler::BatteryData> BatterySampler::MaybeGetBatteryData(
    io_service_t power_source) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> dict;
  kern_return_t result = IORegistryEntryCreateCFProperties(
      power_source, dict.InitializeInto(), 0, 0);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "IORegistryEntryCreateCFProperties";
    return std::nullopt;
  }

  std::optional<bool> external_connected =
      GetValueAsBoolean(dict.get(), CFSTR("ExternalConnected"));
  std::optional<SInt64> voltage_mv =
      GetValueAsSInt64(dict.get(), CFSTR(kIOPSVoltageKey));
  std::optional<SInt64> current_capacity_mah =
      GetValueAsSInt64(dict.get(), CFSTR("AppleRawCurrentCapacity"));
  std::optional<SInt64> max_capacity_mah =
      GetValueAsSInt64(dict.get(), CFSTR("AppleRawMaxCapacity"));
  std::optional<SInt64> update_time =
      GetValueAsSInt64(dict.get(), CFSTR("UpdateTime"));

  if (!external_connected.has_value() || !voltage_mv.has_value() ||
      !current_capacity_mah.has_value() || !max_capacity_mah.has_value()) {
    return std::nullopt;
  }

  BatteryData data{.external_connected = external_connected.value(),
                   .voltage_mv = voltage_mv.value(),
                   .current_capacity_mah = current_capacity_mah.value(),
                   .max_capacity_mah = max_capacity_mah.value(),
                   .update_time_seconds_since_epoch = update_time.value()};

  return data;
}

//  static
std::optional<BatterySampler::AvgConsumption>
BatterySampler::MaybeComputeAvgConsumption(base::TimeDelta duration,
                                           const BatteryData& prev_data,
                                           const BatteryData& new_data) {
  // The gauging hardware measures current consumed (or charged), but reports
  // the remaining capacity with respect to a load-dependent max capacity.
  // Here, however, we care about the delta capacity consumed rather than the
  // capacity remaining. To get to capacity consumed, we flip the capacity
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
    return std::nullopt;

  double avg_voltage_v =
      (new_data.voltage_mv + prev_data.voltage_mv) / (2.0 * 1000.0);

  constexpr double kAsPerMAh =
      static_cast<double>(base::TimeTicks::kSecondsPerHour) / 1000.0;
  double avg_current_a =
      delta_current_consumed_mah * kAsPerMAh / duration.InSecondsF();

  // Arbitrarily use positive values to represent energy being consumed
  // (charging the battery will produce negative values).
  return AvgConsumption{.watts = avg_voltage_v * -avg_current_a,
                        .mah = -delta_current_consumed_mah};
}

// static
std::unique_ptr<BatterySampler> BatterySampler::CreateImpl(
    MaybeGetBatteryDataFn maybe_get_battery_data_fn,
    GetSecondsSinceEpochFn get_seconds_since_epoch_fn,
    base::mac::ScopedIOObject<io_service_t> power_source) {
  // Validate that we can work with this source.
  auto battery_data = maybe_get_battery_data_fn(power_source.get());
  if (!battery_data.has_value())
    return nullptr;

  return base::WrapUnique(
      new BatterySampler(maybe_get_battery_data_fn, get_seconds_since_epoch_fn,
                         std::move(power_source), *battery_data));
}

BatterySampler::BatterySampler(
    MaybeGetBatteryDataFn maybe_get_battery_data_fn,
    GetSecondsSinceEpochFn get_seconds_since_epoch_fn,
    base::mac::ScopedIOObject<io_service_t> power_source,
    BatteryData initial_battery_data)
    : maybe_get_battery_data_fn_(maybe_get_battery_data_fn),
      get_seconds_since_epoch_fn_(get_seconds_since_epoch_fn),
      power_source_(std::move(power_source)) {}

void BatterySampler::StoreBatteryData(base::TimeTicks sample_time,
                                      const BatteryData& battery_data) {
  prev_battery_sample_time_ = sample_time;
  prev_battery_data_ = battery_data;
}

}  // namespace power_sampler
