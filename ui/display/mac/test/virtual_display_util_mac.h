// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_UTIL_MAC_H_
#define UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_UTIL_MAC_H_

#import <IOKit/pwr_mgt/IOPMLib.h>
#include <stdint.h>

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/display/types/display_constants.h"

namespace display {
class Display;
class Screen;

namespace test {

// This interface creates system-level virtual displays to support the automated
// integration testing of display information and window management APIs in
// multi-screen device environments. It updates the displays that the normal mac
// screen impl sees, but not `TestScreenMac`.
class VirtualDisplayUtilMac : public VirtualDisplayUtil,
                              public display::DisplayObserver {
 public:
  explicit VirtualDisplayUtilMac(Screen* screen);
  ~VirtualDisplayUtilMac() override;

  VirtualDisplayUtilMac(const VirtualDisplayUtilMac&) = delete;
  VirtualDisplayUtilMac& operator=(const VirtualDisplayUtilMac&) = delete;

  // VirtualDisplayUtil overrides:
  int64_t AddDisplay(const DisplayParams& display_params) override;
  void RemoveDisplay(int64_t display_id) override;
  void ResetDisplays() override;

  // Check whether the related CoreGraphics APIs are available in the current
  // system version.
  static bool IsAPIAvailable();

  // Preset display configuration parameters.
  static const DisplayParams k6016x3384;
  static const DisplayParams k5120x2880;
  static const DisplayParams k4096x2304;
  static const DisplayParams k3840x2400;
  static const DisplayParams k3840x2160;
  static const DisplayParams k3840x1600;
  static const DisplayParams k3840x1080;
  static const DisplayParams k3072x1920;
  static const DisplayParams k2880x1800;
  static const DisplayParams k2560x1600;
  static const DisplayParams k2560x1440;
  static const DisplayParams k2304x1440;
  static const DisplayParams k2048x1536;
  static const DisplayParams k2048x1152;
  static const DisplayParams k1920x1200;
  static const DisplayParams k1600x1200;
  static const DisplayParams k1920x1080;
  static const DisplayParams k1680x1050;
  static const DisplayParams k1440x900;
  static const DisplayParams k1400x1050;
  static const DisplayParams k1366x768;
  static const DisplayParams k1280x1024;
  static const DisplayParams k1280x1800;

 private:
  class DisplaySleepBlocker {
   public:
    DisplaySleepBlocker();
    ~DisplaySleepBlocker();

    DisplaySleepBlocker(const DisplaySleepBlocker&) = delete;
    DisplaySleepBlocker& operator=(const DisplaySleepBlocker&) = delete;

   private:
    // Track the AssertionID argument to IOPMAssertionCreateWithProperties and
    // IOPMAssertionRelease.
    IOPMAssertionID assertion_id_ = kIOPMNullAssertionID;
  };

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  void OnDisplayAddedOrRemoved(int64_t id);

  // Add a new display with a given serial number.
  int64_t AddDisplay(int64_t serial_number,
                     const DisplayParams& display_params);

  // Wait for the display with the given `id` to be added.
  // Return immediately if the display is already available.
  void WaitForDisplay(int64_t id, bool added);

  void StartWaiting();
  void StopWaiting();

  raw_ptr<Screen> screen_;
  base::flat_set<int64_t> waiting_for_ids_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DisplaySleepBlocker display_sleep_blocker_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_MAC_TEST_VIRTUAL_DISPLAY_UTIL_MAC_H_
