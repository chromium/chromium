// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_CURSOR_CLIENT_H_
#define UI_AURA_TEST_TEST_CURSOR_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/client/cursor_client.h"

namespace ui {
class KeyEvent;
enum class CursorSize;
}

namespace aura {
namespace test {

class TestCursorClient : public aura::client::CursorClient {
 public:
  explicit TestCursorClient(aura::Window* root_window);

  TestCursorClient(const TestCursorClient&) = delete;
  TestCursorClient& operator=(const TestCursorClient&) = delete;

  ~TestCursorClient() override;

  // Used to track the number of times SetCursor() was called.
  int calls_to_set_cursor() const { return calls_to_set_cursor_; }
  void reset_calls_to_set_cursor() { calls_to_set_cursor_ = 0; }

  // Set whether or not to hide cursor on key events.
  void set_should_hide_cursor_on_key_event(bool hide) {
    should_hide_cursor_on_key_event_ = hide;
  }

  // aura::client::CursorClient:
  void SetCursor(gfx::NativeCursor cursor) override;
  gfx::NativeCursor GetCursor() const override;
  void SetCursorForced(gfx::NativeCursor cursor) override;
  void ShowCursor() override;
  void HideCursor() override;
  void SetCursorSize(ui::CursorSize cursor_size) override;
  ui::CursorSize GetCursorSize() const override;
  bool IsCursorVisible() const override;
  void EnableMouseEvents() override;
  void DisableMouseEvents() override;
  bool IsMouseEventsEnabled() const override;
  void SetDisplay(const display::Display& display) override;
  const display::Display& GetDisplay() const override;
  void LockCursor() override;
  void UnlockCursor() override;
  bool IsCursorLocked() const override;
  void AddObserver(aura::client::CursorClientObserver* observer) override;
  void RemoveObserver(aura::client::CursorClientObserver* observer) override;
  bool ShouldHideCursorOnKeyEvent(const ui::KeyEvent& event) const override;
  bool ShouldHideCursorOnTouchEvent(const ui::TouchEvent& event) const override;
  gfx::Size GetSystemCursorSize() const override;

 private:
  bool visible_;
  bool should_hide_cursor_on_key_event_;
  bool mouse_events_enabled_;
  int cursor_lock_count_;
  int calls_to_set_cursor_;
  base::ObserverList<aura::client::CursorClientObserver>::Unchecked observers_;
  raw_ptr<aura::Window> root_window_;
};

}  // namespace test
}  // namespace aura

#endif // UI_AURA_TEST_TEST_CURSOR_CLIENT_H_
