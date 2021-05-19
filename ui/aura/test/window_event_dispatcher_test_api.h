// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_
#define UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_

#include "base/macros.h"

namespace aura {

class WindowEventDispatcher;

namespace test {

class WindowEventDispatcherTestApi {
 public:
  explicit WindowEventDispatcherTestApi(WindowEventDispatcher* dispatcher);

  bool HoldingPointerMoves() const;

  // If pointer moves are being held, this method waits until they're
  // dispatched.
  void WaitUntilPointerMovesDispatched();

 private:
  WindowEventDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventDispatcherTestApi);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_EVENT_DISPATCHER_TEST_API_H_
