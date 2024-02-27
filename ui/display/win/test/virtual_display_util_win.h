// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_
#define UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "third_party/win_virtual_display/controller/display_driver_controller.h"
#include "third_party/win_virtual_display/driver/public/properties.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/virtual_display_util.h"

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
  ~VirtualDisplayUtilWin() override;

  // Check whether the related drivers are available on the current system.
  static bool IsAPIAvailable();

  // VirtualDisplayUtil overrides:
  int64_t AddDisplay(uint8_t id, const DisplayParams& display_params) override;
  void RemoveDisplay(int64_t display_id) override;
  void ResetDisplays() override;
  static const DisplayParams k1920x1080;
  static const DisplayParams k1024x768;

 private:
  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  bool SetDriverProperties(DriverProperties properties);
  void OnDisplayAddedOrRemoved(int64_t id);
  // Start waiting for the detected displays to match `current_config_`.
  void StartWaiting();
  void StopWaiting();

  raw_ptr<Screen> screen_;
  // True if the environment was considered headless during initialization.
  const bool is_headless_;
  std::unique_ptr<base::RunLoop> run_loop_;
  DisplayDriverController driver_controller_;
  // Contains the last configuration that was set.
  DriverProperties current_config_;
  // Map of virtual display ID (product code) to corresponding display ID.
  base::flat_map<unsigned short, int64_t> virtual_displays_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_WIN_TEST_VIRTUAL_DISPLAY_UTIL_WIN_H_
