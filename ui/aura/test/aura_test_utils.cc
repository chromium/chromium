// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_utils.h"
#include "base/memory/raw_ptr.h"

#include <utility>

#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace test {

class WindowTreeHostTestApi {
 public:
  explicit WindowTreeHostTestApi(WindowTreeHost* host) : host_(host) {}

  WindowTreeHostTestApi(const WindowTreeHostTestApi&) = delete;
  WindowTreeHostTestApi& operator=(const WindowTreeHostTestApi&) = delete;

  const gfx::Point& last_cursor_request_position_in_host() {
    return host_->last_cursor_request_position_in_host_;
  }

  void set_dispatcher(std::unique_ptr<WindowEventDispatcher> dispatcher) {
    host_->dispatcher_ = std::move(dispatcher);
  }

  void disable_ime() { host_->dispatcher_->set_skip_ime(true); }

  bool accelerated_widget_made_visible() {
    return host_->accelerated_widget_made_visible_;
  }

  static const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>&
  GetThrottledHosts() {
    return WindowTreeHost::GetThrottledHostsForTesting();
  }

 private:
  raw_ptr<WindowTreeHost> host_;
};

const gfx::Point& QueryLatestMousePositionRequestInHost(WindowTreeHost* host) {
  WindowTreeHostTestApi host_test_api(host);
  return host_test_api.last_cursor_request_position_in_host();
}

void SetHostDispatcher(WindowTreeHost* host,
                       std::unique_ptr<WindowEventDispatcher> dispatcher) {
  WindowTreeHostTestApi host_test_api(host);
  host_test_api.set_dispatcher(std::move(dispatcher));
}

void DisableIME(WindowTreeHost* host) {
  WindowTreeHostTestApi(host).disable_ime();
}

void DisableNativeWindowOcclusionTracking(WindowTreeHost* host) {
  NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(host);
}

const base::flat_set<raw_ptr<WindowTreeHost, CtnExperimental>>&
GetThrottledHosts() {
  return WindowTreeHostTestApi::GetThrottledHosts();
}

bool AcceleratedWidgetMadeVisible(WindowTreeHost* host) {
  return WindowTreeHostTestApi(host).accelerated_widget_made_visible();
}

}  // namespace test
}  // namespace aura
