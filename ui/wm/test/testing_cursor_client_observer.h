// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_TEST_TESTING_CURSOR_CLIENT_OBSERVER_H_
#define UI_WM_TEST_TESTING_CURSOR_CLIENT_OBSERVER_H_

#include "ui/aura/client/cursor_client_observer.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/wm/core/cursor_manager.h"

namespace wm {

// CursorClientObserver for testing.
class TestingCursorClientObserver : public aura::client::CursorClientObserver {
 public:
  TestingCursorClientObserver();

  TestingCursorClientObserver(const TestingCursorClientObserver&) = delete;
  TestingCursorClientObserver& operator=(const TestingCursorClientObserver&) =
      delete;

  void reset();

  bool is_cursor_visible() const { return cursor_visibility_; }
  bool did_visibility_change() const { return did_visibility_change_; }
  ui::CursorSize cursor_size() const { return cursor_size_; }
  bool did_cursor_size_change() const { return did_cursor_size_change_; }

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;
  void OnCursorSizeChanged(ui::CursorSize cursor_size) override;

 private:
  bool cursor_visibility_;
  bool did_visibility_change_;
  ui::CursorSize cursor_size_;
  bool did_cursor_size_change_;
};

}  // namespace wm

#endif  // UI_WM_TEST_TESTING_CURSOR_CLIENT_OBSERVER_H_
