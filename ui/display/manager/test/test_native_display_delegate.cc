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
      run_async_(false),
      log_(log) {}

TestNativeDisplayDelegate::~TestNativeDisplayDelegate() {}

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

bool TestNativeDisplayDelegate::Configure(const DisplaySnapshot& output,
                                          const DisplayMode* mode,
                                          const gfx::Point& origin) {
  log_->AppendAction(GetCrtcAction(output, mode, origin));

  if (max_configurable_pixels_ == 0)
    return true;

  if (!mode)
    return false;

  return mode->size().GetArea() <= max_configurable_pixels_;
}

void TestNativeDisplayDelegate::Configure(const DisplaySnapshot& output,
                                          const DisplayMode* mode,
                                          const gfx::Point& origin,
                                          ConfigureCallback callback) {
  bool result = Configure(output, mode, origin);
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  } else {
    std::move(callback).Run(result);
  }
}

void TestNativeDisplayDelegate::GetHDCPState(const DisplaySnapshot& output,
                                             GetHDCPStateCallback callback) {
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), get_hdcp_expectation_,
                                  hdcp_state_));
  } else {
    std::move(callback).Run(get_hdcp_expectation_, hdcp_state_);
  }
}

void TestNativeDisplayDelegate::SetHDCPState(const DisplaySnapshot& output,
                                             HDCPState state,
                                             SetHDCPStateCallback callback) {
  if (run_async_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TestNativeDisplayDelegate::DoSetHDCPState,
                                  base::Unretained(this), output.display_id(),
                                  state, std::move(callback)));
  } else {
    DoSetHDCPState(output.display_id(), state, std::move(callback));
  }
}

void TestNativeDisplayDelegate::DoSetHDCPState(int64_t display_id,
                                               HDCPState state,
                                               SetHDCPStateCallback callback) {
  log_->AppendAction(GetSetHDCPStateAction(display_id, state));

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
