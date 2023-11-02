// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_
#define UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_

#include "base/memory/raw_ptr.h"
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

  ScopedWindowEventTargetingBlocker(const ScopedWindowEventTargetingBlocker&) =
      delete;
  ScopedWindowEventTargetingBlocker& operator=(
      const ScopedWindowEventTargetingBlocker&) = delete;

  ~ScopedWindowEventTargetingBlocker() override;

  // WindowObserver:
  void OnWindowDestroying(Window* window) override;

 private:
  raw_ptr<Window> window_;
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_WINDOW_EVENT_TARGETING_BLOCKER_H_
