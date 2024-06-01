// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_TEST_NATIVE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_MANAGER_TEST_TEST_NATIVE_DISPLAY_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

class ActionLogger;
class DisplaySnapshot;
class NativeDisplayObserver;

namespace test {

constexpr char kTestModesetStr[] = "test-modeset";
constexpr char kCommitModesetStr[] = "commit-modeset";
constexpr char kSeamlessModesetStr[] = "seamless-modeset";
constexpr char kModesetOutcomeFailure[] = "outcome: failure";
constexpr char kModesetOutcomeSuccess[] = "outcome: success";

class TestNativeDisplayDelegate : public NativeDisplayDelegate {
 public:
  // Ownership of |log| remains with the caller.
  explicit TestNativeDisplayDelegate(ActionLogger* log);

  TestNativeDisplayDelegate(const TestNativeDisplayDelegate&) = delete;
  TestNativeDisplayDelegate& operator=(const TestNativeDisplayDelegate&) =
      delete;

  ~TestNativeDisplayDelegate() override;

  void set_max_configurable_pixels(int pixels) {
    max_configurable_pixels_ = pixels;
  }

  void set_system_bandwidth_limit(int bandwidth_limit) {
    system_bandwidth_limit_ = bandwidth_limit;
  }

  void set_get_hdcp_state_expectation(bool success) {
    get_hdcp_expectation_ = success;
  }

  void set_set_hdcp_state_expectation(bool success) {
    set_hdcp_expectation_ = success;
  }

  HDCPState hdcp_state() const { return hdcp_state_; }
  void set_hdcp_state(HDCPState state) { hdcp_state_ = state; }

  ContentProtectionMethod content_protection_method() const {
    return content_protection_method_;
  }
  void set_content_protection_method(
      ContentProtectionMethod protection_method) {
    content_protection_method_ = protection_method;
  }

  void set_run_async(bool run_async) { run_async_ = run_async; }

  const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> GetOutputs()
      const;

  // Sets and takes ownership of the provided |outputs|.
  void SetOutputs(std::vector<std::unique_ptr<DisplaySnapshot>> outputs);

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void TakeDisplayControl(DisplayControlCallback callback) override;
  void RelinquishDisplayControl(DisplayControlCallback callback) override;
  void GetDisplays(GetDisplaysCallback callback) override;
  void Configure(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      ConfigureCallback callback,
      display::ModesetFlags modeset_flags) override;
  void SetHdcpKeyProp(int64_t display_id,
                      const std::string& key,
                      SetHdcpKeyPropCallback callback) override;
  void GetHDCPState(const DisplaySnapshot& output,
                    GetHDCPStateCallback callback) override;
  void SetHDCPState(const DisplaySnapshot& output,
                    HDCPState state,
                    ContentProtectionMethod protection_method,
                    SetHDCPStateCallback callback) override;
  void SetColorTemperatureAdjustment(
      int64_t display_id,
      const ColorTemperatureAdjustment& cta) override;
  void SetColorCalibration(int64_t display_id,
                           const ColorCalibration& calibration) override;
  void SetGammaAdjustment(int64_t display_id,
                          const GammaAdjustment& gamma) override;
  void SetPrivacyScreen(int64_t display_id,
                        bool enabled,
                        SetPrivacyScreenCallback callback) override;
  void GetSeamlessRefreshRates(
      int64_t display_id,
      GetSeamlessRefreshRatesCallback callback) const override;

  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;
  FakeDisplayController* GetFakeDisplayController() override;

 private:
  bool Configure(
      const display::DisplayConfigurationParams& display_config_params);

  void DoSetHDCPState(int64_t display_id,
                      HDCPState state,
                      ContentProtectionMethod protection_method,
                      SetHDCPStateCallback callback);

  bool IsConfigurationWithinSystemBandwidth(
      const std::vector<display::DisplayConfigurationParams>& config_requests);
  void SaveCurrentConfigSystemBandwidth(
      const std::vector<display::DisplayConfigurationParams>& config_requests);

  // Outputs to be returned by GetDisplays().
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs_;
  // Outputs which are scheduled for deletion after the next invalidation.
  std::vector<std::unique_ptr<DisplaySnapshot>> cached_outputs_;

  // |max_configurable_pixels_| represents the maximum number of pixels that
  // Configure will support.  Tests can use this to force Configure
  // to fail if attempting to set a resolution that is higher than what
  // a device might support under a given circumstance.
  // A value of 0 means that no limit is enforced and Configure will
  // return success regardless of the resolution.
  int max_configurable_pixels_;

  int system_bandwidth_limit_ = 0;
  base::flat_map<int64_t, int> display_id_to_used_system_bw_;

  bool get_hdcp_expectation_;
  bool set_hdcp_expectation_;

  // Result value of GetHDCPState().
  HDCPState hdcp_state_;
  ContentProtectionMethod content_protection_method_;

  // If true, the callbacks are posted on the message loop.
  bool run_async_;

  raw_ptr<ActionLogger, DanglingUntriaged> log_;  // Not owned.

  base::ObserverList<NativeDisplayObserver>::Unchecked observers_;
};

}  // namespace test

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_TEST_NATIVE_DISPLAY_DELEGATE_H_
