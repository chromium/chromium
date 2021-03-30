// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/test_native_display_delegate.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"

namespace display {
namespace test {

TestNativeDisplayDelegate::TestNativeDisplayDelegate(ActionLogger* log)
    : max_configurable_pixels_(0),
      get_hdcp_expectation_(true),
      set_hdcp_expectation_(true),
      hdcp_state_(HDCP_STATE_UNDESIRED),
      content_protection_method_(CONTENT_PROTECTION_METHOD_NONE),
      run_async_(false),
      log_(log) {}

TestNativeDisplayDelegate::~TestNativeDisplayDelegate() = default;

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
  for (NativeDisplayObserver& observer : observers_)
    observer.OnDisplaySnapshotsInvalidated();

  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), outputs_));
  } else {
    std::move(callback).Run(outputs_);
  }
}

bool TestNativeDisplayDelegate::Configure(
    const display::DisplayConfigurationParams& display_config_params) {
  log_->AppendAction(GetCrtcAction(display_config_params));

  if (max_configurable_pixels_ == 0)
    return true;

  if (!display_config_params.mode.has_value())
    return false;

  return display_config_params.mode.value()->size().GetArea() <=
         max_configurable_pixels_;
}

void TestNativeDisplayDelegate::Configure(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    ConfigureCallback callback) {
  bool config_success = true;
  for (const auto& config : config_requests)
    config_success &= Configure(config);

  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), config_success));
  } else {
    std::move(callback).Run(config_success);
  }
}

void TestNativeDisplayDelegate::GetHDCPState(const DisplaySnapshot& output,
                                             GetHDCPStateCallback callback) {
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
      NOTREACHED();
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

bool TestNativeDisplayDelegate::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  log_->AppendAction(SetColorMatrixAction(display_id, color_matrix));
  return true;
}

bool TestNativeDisplayDelegate::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  log_->AppendAction(
      SetGammaCorrectionAction(display_id, degamma_lut, gamma_lut));
  return true;
}

void TestNativeDisplayDelegate::SetPrivacyScreen(int64_t display_id,
                                                 bool enabled) {
  log_->AppendAction(SetPrivacyScreenAction(display_id, enabled));
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

}  // namespace test
}  // namespace display
