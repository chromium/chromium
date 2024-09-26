// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/test_native_display_delegate.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/size.h"

namespace display::test {

std::string GetModesetFlag(display::ModesetFlags modeset_flags) {
  std::string flags_str;
  if (modeset_flags.Has(display::ModesetFlag::kTestModeset)) {
    flags_str = base::StrCat({flags_str, kTestModesetStr, ","});
  }
  if (modeset_flags.Has(display::ModesetFlag::kCommitModeset)) {
    flags_str = base::StrCat({flags_str, kCommitModesetStr, ","});
  }
  if (modeset_flags.Has(display::ModesetFlag::kSeamlessModeset)) {
    flags_str = base::StrCat({flags_str, kSeamlessModesetStr, ","});
  }

  // Remove trailing comma.
  if (!flags_str.empty())
    flags_str.resize(flags_str.size() - 1);
  return flags_str;
}

TestNativeDisplayDelegate::TestNativeDisplayDelegate(ActionLogger* log)
    : max_configurable_pixels_(0),
      get_hdcp_expectation_(true),
      set_hdcp_expectation_(true),
      hdcp_state_(HDCP_STATE_UNDESIRED),
      content_protection_method_(CONTENT_PROTECTION_METHOD_NONE),
      run_async_(false),
      log_(log) {}

TestNativeDisplayDelegate::~TestNativeDisplayDelegate() = default;

const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>
TestNativeDisplayDelegate::GetOutputs() const {
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> outputs;
  for (const auto& output : outputs_) {
    outputs.push_back(output.get());
  }
  return outputs;
}

void TestNativeDisplayDelegate::SetOutputs(
    std::vector<std::unique_ptr<DisplaySnapshot>> outputs) {
  std::move(begin(outputs_), end(outputs_),
            std::back_inserter(cached_outputs_));
  outputs_ = std::move(outputs);
}

void TestNativeDisplayDelegate::Initialize() {
  log_->AppendAction(kInit);
}

void TestNativeDisplayDelegate::TakeDisplayControl(
    DisplayControlCallback callback) {
  log_->AppendAction(kTakeDisplayControl);
  std::move(callback).Run(true);
}

void TestNativeDisplayDelegate::RelinquishDisplayControl(
    DisplayControlCallback callback) {
  log_->AppendAction(kRelinquishDisplayControl);
  std::move(callback).Run(true);
}

void TestNativeDisplayDelegate::GetDisplays(GetDisplaysCallback callback) {
  // This mimics the behavior of Ozone DRM when new display state arrives.
  observers_.Notify(&NativeDisplayObserver::OnDisplaySnapshotsInvalidated);
  cached_outputs_.clear();

  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetOutputs()));
  } else {
    std::move(callback).Run(GetOutputs());
  }
}

bool TestNativeDisplayDelegate::Configure(
    const display::DisplayConfigurationParams& display_config_params) {
  log_->AppendAction(GetCrtcAction(display_config_params));

  if (max_configurable_pixels_ == 0)
    return true;
  else if (max_configurable_pixels_ < 0)
    return false;

  if (display_config_params.mode) {
    return display_config_params.mode->size().GetArea() <=
           max_configurable_pixels_;
  }

  return true;
}

bool TestNativeDisplayDelegate::IsConfigurationWithinSystemBandwidth(
    const std::vector<display::DisplayConfigurationParams>& config_requests) {
  if (system_bandwidth_limit_ == 0)
    return true;

  // We need a copy of the current state to account for current configuration.
  // But we can't overwrite it yet because we may fail to configure
  base::flat_map<int64_t, int> requested_ids_with_bandwidth =
      display_id_to_used_system_bw_;
  for (const DisplayConfigurationParams& config : config_requests) {
    requested_ids_with_bandwidth[config.id] =
        config.mode ? config.mode->size().GetArea() : 0;
  }

  int requested_bandwidth = 0;
  for (const auto& it : requested_ids_with_bandwidth) {
    requested_bandwidth += it.second;
  }

  return requested_bandwidth <= system_bandwidth_limit_;
}

void TestNativeDisplayDelegate::SaveCurrentConfigSystemBandwidth(
    const std::vector<display::DisplayConfigurationParams>& config_requests) {
  // On a successful configuration, we update the current state to reflect the
  // current system usage.
  for (const DisplayConfigurationParams& config : config_requests) {
    display_id_to_used_system_bw_[config.id] =
        config.mode ? config.mode->size().GetArea() : 0;
  }
}

void TestNativeDisplayDelegate::Configure(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    ConfigureCallback callback,
    display::ModesetFlags modeset_flags) {
  log_->AppendAction(GetModesetFlag(modeset_flags));
  bool config_success = true;
  for (const auto& config : config_requests)
    config_success &= Configure(config);

  config_success &= IsConfigurationWithinSystemBandwidth(config_requests);

  if (config_success)
    SaveCurrentConfigSystemBandwidth(config_requests);

  std::string config_outcome = "outcome: ";
  config_outcome += config_success ? "success" : "failure";
  log_->AppendAction(config_outcome);

  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), config_requests, config_success));
  } else {
    std::move(callback).Run(config_requests, config_success);
  }
}

void TestNativeDisplayDelegate::SetHdcpKeyProp(
    int64_t display_id,
    const std::string& key,
    SetHdcpKeyPropCallback callback) {
  log_->AppendAction(GetSetHdcpKeyPropAction(display_id, true));

  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  } else {
    std::move(callback).Run(true);
  }
}

void TestNativeDisplayDelegate::GetHDCPState(const DisplaySnapshot& output,
                                             GetHDCPStateCallback callback) {
  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), get_hdcp_expectation_,
                                  hdcp_state_, content_protection_method_));
  } else {
    std::move(callback).Run(get_hdcp_expectation_, hdcp_state_,
                            content_protection_method_);
  }
}

void TestNativeDisplayDelegate::SetHDCPState(
    const DisplaySnapshot& output,
    HDCPState state,
    ContentProtectionMethod protection_method,
    SetHDCPStateCallback callback) {
  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestNativeDisplayDelegate::DoSetHDCPState,
                       base::Unretained(this), output.display_id(), state,
                       protection_method, std::move(callback)));
  } else {
    DoSetHDCPState(output.display_id(), state, protection_method,
                   std::move(callback));
  }
}

void TestNativeDisplayDelegate::DoSetHDCPState(
    int64_t display_id,
    HDCPState state,
    ContentProtectionMethod protection_method,
    SetHDCPStateCallback callback) {
  log_->AppendAction(
      GetSetHDCPStateAction(display_id, state, protection_method));

  switch (state) {
    case HDCP_STATE_ENABLED:
      NOTREACHED_IN_MIGRATION();
      break;

    case HDCP_STATE_DESIRED:
      hdcp_state_ =
          set_hdcp_expectation_ ? HDCP_STATE_ENABLED : HDCP_STATE_DESIRED;
      break;

    case HDCP_STATE_UNDESIRED:
      if (set_hdcp_expectation_)
        hdcp_state_ = HDCP_STATE_UNDESIRED;
      break;
  }

  content_protection_method_ = set_hdcp_expectation_
                                   ? protection_method
                                   : CONTENT_PROTECTION_METHOD_NONE;

  std::move(callback).Run(set_hdcp_expectation_);
}

void TestNativeDisplayDelegate::SetColorCalibration(
    int64_t display_id,
    const ColorCalibration& calibration) {
  log_->AppendAction(SetColorCalibrationAction(display_id, calibration));
}

void TestNativeDisplayDelegate::SetColorTemperatureAdjustment(
    int64_t display_id,
    const ColorTemperatureAdjustment& cta) {
  log_->AppendAction(SetColorTemperatureAdjustmentAction(display_id, cta));
}

void TestNativeDisplayDelegate::SetGammaAdjustment(
    int64_t display_id,
    const GammaAdjustment& gamma) {
  log_->AppendAction(SetGammaAdjustmentAction(display_id, gamma));
}

void TestNativeDisplayDelegate::SetPrivacyScreen(
    int64_t display_id,
    bool enabled,
    SetPrivacyScreenCallback callback) {
  log_->AppendAction(SetPrivacyScreenAction(display_id, enabled));
  std::move(callback).Run(true);
}

void TestNativeDisplayDelegate::GetSeamlessRefreshRates(
    int64_t display_id,
    GetSeamlessRefreshRatesCallback callback) const {
  const DisplaySnapshot* snapshot = nullptr;
  for (const auto& output : outputs_) {
    if (output->display_id() == display_id) {
      snapshot = output.get();
      break;
    }
  }
  // Return nullopt if there is no snapshot with this display_id.
  std::optional<std::vector<float>> result;
  if (snapshot) {
    // Return empty vector if there is no current mode.
    std::vector<float> refresh_rates;
    if (snapshot->current_mode()) {
      for (auto& mode : snapshot->modes()) {
        // If a mode has the same size as the currently configured mode, then
        // include that mode's refresh rate.
        if (mode->size() == snapshot->current_mode()->size()) {
          refresh_rates.push_back(mode->refresh_rate());
        }
      }
    }
    result.emplace(refresh_rates);
  }

  if (run_async_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  } else {
    std::move(callback).Run(result);
  }
}

void TestNativeDisplayDelegate::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void TestNativeDisplayDelegate::RemoveObserver(
    NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

FakeDisplayController* TestNativeDisplayDelegate::GetFakeDisplayController() {
  return nullptr;
}

}  // namespace display::test
