// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_
#define UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "third_party/win_virtual_display/controller/display_driver_controller.h"
#include "third_party/win_virtual_display/driver/public/properties.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display {
class Display;
class Screen;

namespace test {
struct DisplayParams;

// This interface creates system-level virtual displays to support the automated
// integration testing of display information and window management APIs in
// multi-screen device environments. It updates the displays that the normal
// windows screen impl sees.
class VirtualDisplayUtilWin : public display::DisplayObserver,
                              public VirtualDisplayUtil {
 public:
  explicit VirtualDisplayUtilWin(Screen* screen);
  VirtualDisplayUtilWin(const VirtualDisplayUtilWin&) = delete;
  VirtualDisplayUtilWin& operator=(const VirtualDisplayUtilWin&) = delete;
  ~VirtualDisplayUtilWin() override;

  // Check whether the related drivers are available on the current system.
  static bool IsAPIAvailable();

  // VirtualDisplayUtil overrides:
  int64_t AddDisplay(const DisplayParams& display_params) override;
  void RemoveDisplay(int64_t display_id) override;
  void ResetDisplays() override;

 private:
  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  bool SetDriverProperties(DriverProperties properties);
  void OnDisplayAddedOrRemoved(int64_t id);
  // Start waiting for the detected displays to match `current_config_`.
  void StartWaiting();
  void StopWaiting();

  // Ensures that display topology is in extend mode (not mirror).
  void EnsureExtendMode();

  // Creates a new internal display ID to identify the display to the driver.
  static uint8_t SynthesizeInternalDisplayId();

  raw_ptr<Screen> screen_;
  // True if the environment was considered headless during initialization.
  const bool is_headless_;
  std::unique_ptr<base::RunLoop> run_loop_;
  // Periodically ensures that display topology is in extended mode.
  base::RepeatingTimer ensure_extended_timer_;
  DisplayDriverController driver_controller_;
  // Contains the last configuration that was set.
  DriverProperties current_config_;
  // Map of internal display ID (product code) to corresponding display ID.
  base::flat_map<unsigned short, int64_t> virtual_displays_;
  // Copy of the display list when this utility was constructed.
  std::vector<display::Display> initial_displays_;
};

// Utility that waits until desired display configs are observed.
// TODO(btriebw): Generalize this class and share it among VirtualDisplayUtil
// classes.
class DisplayConfigWaiter : public display::DisplayObserver {
 public:
  explicit DisplayConfigWaiter(Screen* screen);
  DisplayConfigWaiter(const VirtualDisplayUtilWin&) = delete;
  DisplayConfigWaiter& operator=(const VirtualDisplayUtilWin&) = delete;
  ~DisplayConfigWaiter() override;

  // Waits until the display configuration equals the specified resolutions, in
  // any arrangement.
  void WaitForDisplaySizes(std::vector<gfx::Size> sizes);

 private:
  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  // Checks if the wait condition is true that should end the wait loop.
  bool IsWaitConditionMet();

  raw_ptr<Screen> screen_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<gfx::Size> wait_for_sizes_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_
