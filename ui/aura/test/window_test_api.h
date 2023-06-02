// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_TEST_API_H_
#define UI_AURA_TEST_WINDOW_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace aura {
namespace test {

class WindowTestApi {
 public:
  explicit WindowTestApi(Window* window);

  WindowTestApi(const WindowTestApi&) = delete;
  WindowTestApi& operator=(const WindowTestApi&) = delete;

  bool OwnsLayer() const;

  bool ContainsMouse() const;

  void DisableFrameSinkRegistration();

  void SetOcclusionState(aura::Window::OcclusionState state);

 private:
  raw_ptr<Window, DanglingUntriaged> window_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_WINDOW_TEST_API_H_
