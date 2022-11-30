// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_
#define UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_

#include "base/memory/raw_ptr.h"

namespace aura {

class WindowEventDispatcher;

namespace test {

class WindowEventDispatcherTestApi {
 public:
  explicit WindowEventDispatcherTestApi(WindowEventDispatcher* dispatcher);

  WindowEventDispatcherTestApi(const WindowEventDispatcherTestApi&) = delete;
  WindowEventDispatcherTestApi& operator=(const WindowEventDispatcherTestApi&) =
      delete;

  bool HoldingPointerMoves() const;

  // If pointer moves are being held, this method waits until they're
  // dispatched.
  void WaitUntilPointerMovesDispatched();

 private:
  raw_ptr<WindowEventDispatcher> dispatcher_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_
