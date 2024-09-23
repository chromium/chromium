// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_WINDOW_MANAGER_H_
#define UI_OZONE_DEMO_WINDOW_MANAGER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/demo/renderer_factory.h"

namespace display {

class DisplaySnapshot;
class NativeDisplayDelegate;

struct DisplayConfigurationParams;

}  // namespace display

namespace ui {

class DemoWindow;

class WindowManager : public display::NativeDisplayObserver {
 public:
  explicit WindowManager(std::unique_ptr<RendererFactory> renderer_factory,
                         base::OnceClosure quit_closure);

  WindowManager(const WindowManager&) = delete;
  WindowManager& operator=(const WindowManager&) = delete;

  ~WindowManager() override;

  void Quit();

  void AddWindow(DemoWindow* window);
  void RemoveWindow(DemoWindow* window);

 private:
  void OnDisplaysAcquired(
      const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
          displays);
  void OnDisplayConfigured(
      const std::vector<display::DisplayConfigurationParams>& request_results,
      bool config_success);

  // display::NativeDisplayDelegate:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

  std::unique_ptr<display::NativeDisplayDelegate> delegate_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<RendererFactory> renderer_factory_;
  std::vector<std::unique_ptr<DemoWindow>> windows_;

  // Flags used to keep track of the current state of display configuration.
  //
  // True if configuring the displays. In this case a new display configuration
  // isn't started.
  bool is_configuring_ = false;

  // If |is_configuring_| is true and another display configuration event
  // happens, the event is deferred. This is set to true and a display
  // configuration will be scheduled after the current one finishes.
  bool should_configure_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_WINDOW_MANAGER_H_
