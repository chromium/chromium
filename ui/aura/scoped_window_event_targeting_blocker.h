// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_
#define UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"

namespace aura {

class Window;

// Temporarily blocks the event targeting by setting kNone targeting policy to
// |window_|. The original event targeting policy will be restored if all
// targeting blockers are removed from |window_|.
class AURA_EXPORT ScopedWindowEventTargetingBlocker : public WindowObserver {
 public:
  explicit ScopedWindowEventTargetingBlocker(Window* window);
  ~ScopedWindowEventTargetingBlocker() override;

  // WindowObserver:
  void OnWindowDestroying(Window* window) override;

 private:
  Window* window_;
  DISALLOW_COPY_AND_ASSIGN(ScopedWindowEventTargetingBlocker);
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_
