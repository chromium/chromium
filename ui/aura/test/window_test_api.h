// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_TEST_API_H_
#define UI_AURA_TEST_WINDOW_TEST_API_H_

#include "base/macros.h"
#include "ui/aura/window.h"

namespace aura {
namespace test {

class WindowTestApi {
 public:
  explicit WindowTestApi(Window* window);

  bool OwnsLayer() const;

  bool ContainsMouse() const;

  void DisableFrameSinkRegistration();

  void SetOcclusionState(aura::Window::OcclusionState state);

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTestApi);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_TEST_API_H_
