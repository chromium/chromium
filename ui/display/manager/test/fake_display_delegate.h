// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/display/types/fake_display_controller.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

// A NativeDisplayDelegate implementation that manages fake displays. Fake
// displays mimic physical displays but do not actually exist. This is because
// Chrome OS does not take over the entire display when running off device and
// instead runs inside windows provided by the parent OS. Fake displays allow us
// to simulate different connected display states off device and to test display
// configuration and display management code.
//
// The size and number of displays can controlled via --screen-config=X
// command line flag. The format is as follows, where [] are optional:
//   native_mode[#other_modes][^dpi][/options]
//
// native_mode: the native display mode, with format:
//   WxH[%R]
//     W: display width in pixels [int]
//     H: display height in pixels [int]
//     R: display refresh rate [float]
//
// other_modes: list of other of display modes, with format:
//   #WxH[%R][:WxH[%R]]
//     W,H,R: same meaning as in native_mode.
//   Note: The first mode is delimited with '#' and any subsequent modes are
//         delimited with ':'.
//
// dpi: display DPI used to set physical size, with format:
//   ^D
//     D: display DPI [int]
//
// options: options to set on display snapshot, with format:
//   /[a][c][i][o]
//     a: display is aspect preserving [literal a]
//     c: display has color correction matrix [literal c]
//     i: display is internal [literal i]
//     o: display has overscan [literal o]
//
// Examples:
//
// Two 800x800 displays, with first display as internal display:
//  --screen-config=800x800/i,800x800
// One 1600x900 display as internal display with 120 refresh rate and high-DPI:
//  --screen-config=1600x900%120^300/i
// One 1920x1080 display with alternate resolutions:
//  --screen-config=1920x1080#1600x900:1280x720
// No displays:
//  --screen-config=none
//
// FakeDisplayDelegate also implements FakeDisplayController which provides a
// way to change the display state at runtime.
class FakeDisplayDelegate : public NativeDisplayDelegate,
                            public FakeDisplayController {
 public:
  FakeDisplayDelegate();

  FakeDisplayDelegate(const FakeDisplayDelegate&) = delete;
  FakeDisplayDelegate& operator=(const FakeDisplayDelegate&) = delete;

  ~FakeDisplayDelegate() override;

  // FakeDisplayController:
  int64_t AddDisplay(const gfx::Size& display_size) override;
  bool AddDisplay(std::unique_ptr<DisplaySnapshot> display) override;
  bool RemoveDisplay(int64_t display_id) override;

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

 protected:
  // Creates and adds displays based on spec string |str|.
  void CreateDisplaysFromSpecString(const std::string& str);

  // Updates observers when display configuration has changed. Will not update
  // until after |Initialize()| has been called.
  void OnConfigurationChanged();

 private:
  // Performs callback for Configure().
  void ConfigureDone();

  base::ObserverList<NativeDisplayObserver>::Unchecked observers_;
  std::vector<std::unique_ptr<DisplaySnapshot>> displays_;

  // Add delay before finishing Configure() and running callback.
  base::OneShotTimer configure_timer_;
  base::queue<base::OnceClosure> configure_callbacks_;

  // If Initialize() has been called.
  bool initialized_ = false;

  // The next available display id.
  uint8_t next_display_id_ = 0;
};

}  // namespace display
#endif  // UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_DELEGATE_H_
