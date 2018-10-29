// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_SETUP_H_
#define UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_SETUP_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/mus/test_window_tree_delegate.h"

namespace aura {

class TestWindowTree;
class WindowOcclusionTracker;
class WindowTreeClientDelegate;

// TestWindowTreeClientSetup is used to create a WindowTreeClient that is not
// connected to mus.
class TestWindowTreeClientSetup : public TestWindowTreeDelegate {
 public:
  TestWindowTreeClientSetup();
  ~TestWindowTreeClientSetup() override;

  // Initializes the WindowTreeClient.
  void Init(WindowTreeClientDelegate* window_tree_delegate);
  // TODO(sky): see if can combine with Init().
  void InitWithoutEmbed(WindowTreeClientDelegate* window_tree_delegate);

  // The WindowTree that WindowTreeClient talks to.
  TestWindowTree* window_tree() { return window_tree_.get(); }

  // Returns ownership of WindowTreeClient to the caller.
  std::unique_ptr<WindowTreeClient> OwnWindowTreeClient();

  WindowTreeClient* window_tree_client();

 private:
  // Called by both implementations of init to perform common initialization.
  void CommonInit(WindowTreeClientDelegate* window_tree_delegate);

  // TestWindowTreeDelegate:
  void TrackOcclusionState(ws::Id window_id) override;
  void PauseWindowOcclusionTracking() override;
  void UnpauseWindowOcclusionTracking() override;

  // Provide occlusion tracking for simulated server behavior. Needs to be
  // released after |window_tree_client_|.
  std::unique_ptr<WindowOcclusionTracker> window_occlusion_tracker_;

  std::unique_ptr<TestWindowTree> window_tree_;

  std::unique_ptr<WindowTreeClient> window_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClientSetup);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_TEST_WINDOW_TREE_CLIENT_SETUP_H_
