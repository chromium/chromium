// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_TEST_WINDOW_TREE_DELEGATE_H_
#define UI_AURA_TEST_MUS_TEST_WINDOW_TREE_DELEGATE_H_

#include "services/ws/common/types.h"

namespace aura {

// Interface to delegate operations that TestWindowTree could not perform alone.
class TestWindowTreeDelegate {
 public:
  // Invoked to start tracking occlusion state of window.
  virtual void TrackOcclusionState(ws::Id window_id) {}

  // Invoked to pause/unpause window occlusion state computation.
  virtual void PauseWindowOcclusionTracking() {}
  virtual void UnpauseWindowOcclusionTracking() {}

 protected:
  virtual ~TestWindowTreeDelegate() = default;
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_TEST_WINDOW_TREE_DELEGATE_H_
