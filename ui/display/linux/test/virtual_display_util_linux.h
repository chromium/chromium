// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_
#define UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "remoting/host/x11_desktop_resizer.h"
#include "ui/display/display_observer.h"
#include "ui/display/test/virtual_display_util.h"

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
  int64_t AddDisplay(uint8_t id, const DisplayParams& display_params) override;
  void RemoveDisplay(int64_t display_id) override;
  void ResetDisplays() override;

  // These should be a subset of the resolutions configured in //testing/xvfb.py
  static const DisplayParams k800x600;
  static const DisplayParams k1024x768;
  static const DisplayParams k1280x800;
  static const DisplayParams k1920x1080;
  static const DisplayParams k1600x1200;
  static const DisplayParams k3840x2160;

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
  std::unique_ptr<remoting::X11DesktopResizer> desktop_resizer_;
  // Initial layout when this class was instantiated that should be restored.
  remoting::DesktopLayoutSet initial_layout_;
  // Current layout calculated by `desktop_resizer_` after an operation.
  remoting::DesktopLayoutSet current_layout_;
  // Last layout request sent to `desktop_resizer_`.
  remoting::DesktopLayoutSet last_requested_layout_;

  // There are lots of IDS to track here:
  //  1. The user-requested ID set in AddDisplay().
  //  2. The resizer (xrandr) display ID
  //  3. The display ID detected by the display::Screen implementation.
  using RequestedId = uint8_t;
  using ResizerDisplayId = int64_t;
  using DisplayId = int64_t;

  // Queue of displays added via OnDisplayAdded. Removed as they are reconciled
  // and moved to `display_id_to_resizer_id_`.
  base::circular_deque<DisplayId> detected_added_display_ids_;
  base::flat_map<DisplayId, ResizerDisplayId> display_id_to_resizer_id_;

  // Tracks display IDs requested in AddDisplay(). The IDs don't do anything in
  // this implementation, but they are tracked to prevent the user from
  // specifying the same ID twice without deleting it first (to match other
  // platform behavior);
  base::circular_deque<RequestedId> requested_ids_;
  base::flat_map<RequestedId, DisplayId> requested_ids_to_display_ids_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_LINUX_TEST_VIRTUAL_DISPLAY_UTIL_LINUX_H_
