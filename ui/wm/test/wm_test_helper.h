// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_TEST_WM_TEST_HELPER_H_
#define UI_WM_TEST_WM_TEST_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
class TestScreen;
class Window;
class WindowTreeHost;
namespace client {
class DefaultCaptureClient;
class FocusClient;
}
}

namespace gfx {
class Rect;
class Size;
}

namespace wm {

class CompoundEventFilter;
class WMState;

// Creates a minimal environment for running the shell. We can't pull in all of
// ash here, but we can create and attach several of the same things we'd find
// in ash.
class WMTestHelper : public aura::client::WindowParentingClient {
 public:
  explicit WMTestHelper(const gfx::Size& default_window_size);
  ~WMTestHelper() override;

  aura::WindowTreeHost* host() { return host_.get(); }

  // Overridden from client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override;

 private:
  std::unique_ptr<WMState> wm_state_;
  std::unique_ptr<aura::TestScreen> test_screen_;
  std::unique_ptr<aura::WindowTreeHost> host_;
  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;
  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;

  DISALLOW_COPY_AND_ASSIGN(WMTestHelper);
};

}  // namespace wm

#endif  // UI_WM_TEST_WM_TEST_HELPER_H_
