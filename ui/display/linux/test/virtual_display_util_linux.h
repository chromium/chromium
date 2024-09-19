// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_
#define UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/gfx/x/randr_output_manager.h"

namespace display {
class Display;
class Screen;

namespace test {

// Linux implementation of VirtualDisplayUtil. This uses remote desktop code
// (remoting::X11DesktopResizer) to do the X11/XRandR heavy lifting.
class VirtualDisplayUtilLinux : public display::DisplayObserver,
                                public VirtualDisplayUtil {
 public:
  explicit VirtualDisplayUtilLinux(Screen* screen);
  ~VirtualDisplayUtilLinux() override;
  // Maximum number of displays that can be added through AddDisplay().
  // It should be one less than the number of dummy monitors configured in
  // //testing/xvfb.py
  static constexpr int kMaxDisplays = 4;
  // Check whether the related drivers are available on the current system.
  static bool IsAPIAvailable();

  // VirtualDisplayUtil overrides:
  int64_t AddDisplay(const DisplayParams& display_params) override;
  void RemoveDisplay(int64_t display_id) override;
  void ResetDisplays() override;

  // These should be a subset of the resolutions configured in //testing/xvfb.py
  static constexpr DisplayParams k800x600 = {gfx::Size(800, 600)};
  static constexpr DisplayParams k1024x768 = VirtualDisplayUtil::k1024x768;
  static constexpr DisplayParams k1280x800 = {gfx::Size(1280, 800)};
  static constexpr DisplayParams k1920x1080 = VirtualDisplayUtil::k1920x1080;
  static constexpr DisplayParams k1600x1200 = {gfx::Size(1600, 1200)};
  static constexpr DisplayParams k3840x2160 = {gfx::Size(3840, 2160)};

 private:
  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  void OnDisplayAddedOrRemoved(int64_t id);
  bool RequestedLayoutIsSet();
  // Start waiting for the detected displays to match `current_config_`.
  void StartWaiting();
  void StopWaiting();

  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<Screen> screen_;
  std::unique_ptr<x11::RandROutputManager> randr_output_manager_;
  // Initial layout when this class was instantiated that should be restored.
  x11::RandRMonitorLayout initial_layout_;
  // Current layout calculated by `randr_output_manager_` after an operation.
  x11::RandRMonitorLayout current_layout_;
  // Last layout request sent to `randr_output_manager_`.
  x11::RandRMonitorLayout last_requested_layout_;

  using RandrOutputId = int64_t;  // The RandROutputManager output ID
  using DisplayId = int64_t;      // The display ID used by display::Screen.

  // Queue of displays added via OnDisplayAdded. Removed as they are reconciled
  // and moved to `display_id_to_randr_id_`.
  base::circular_deque<DisplayId> detected_added_display_ids_;
  base::flat_map<DisplayId, RandrOutputId> display_id_to_randr_id_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_
