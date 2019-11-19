// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_utils.h"

#include <utility>

#include "base/macros.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace test {

class WindowTreeHostTestApi {
 public:
  explicit WindowTreeHostTestApi(WindowTreeHost* host) : host_(host) {}

  const gfx::Point& last_cursor_request_position_in_host() {
    return host_->last_cursor_request_position_in_host_;
  }

  void set_dispatcher(std::unique_ptr<WindowEventDispatcher> dispatcher) {
    host_->dispatcher_ = std::move(dispatcher);
  }

  void disable_ime() { host_->dispatcher_->set_skip_ime(true); }

 private:
  WindowTreeHost* host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostTestApi);
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

}  // namespace test
}  // namespace aura
