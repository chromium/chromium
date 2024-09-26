// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_cursor_client.h"

#include "ui/aura/client/cursor_client_observer.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"

namespace aura {
namespace test {

TestCursorClient::TestCursorClient(aura::Window* root_window)
    : visible_(true),
      should_hide_cursor_on_key_event_(true),
      mouse_events_enabled_(true),
      cursor_lock_count_(0),
      calls_to_set_cursor_(0),
      root_window_(root_window) {
  client::SetCursorClient(root_window, this);
}

TestCursorClient::~TestCursorClient() {
  client::SetCursorClient(root_window_, NULL);
}

void TestCursorClient::SetCursor(gfx::NativeCursor cursor) {
  calls_to_set_cursor_++;
}

gfx::NativeCursor TestCursorClient::GetCursor() const {
  return ui::mojom::CursorType::kNull;
}

void TestCursorClient::SetCursorForced(gfx::NativeCursor cursor) {
  SetCursor(cursor);
}

void TestCursorClient::ShowCursor() {
  visible_ = true;
  observers_.Notify(
      &aura::client::CursorClientObserver::OnCursorVisibilityChanged, true);
}

void TestCursorClient::HideCursor() {
  visible_ = false;
  observers_.Notify(
      &aura::client::CursorClientObserver::OnCursorVisibilityChanged, false);
}

void TestCursorClient::SetCursorSize(ui::CursorSize cursor_size) {}

ui::CursorSize TestCursorClient::GetCursorSize() const {
  return ui::CursorSize::kNormal;
}

bool TestCursorClient::IsCursorVisible() const {
  return visible_;
}

void TestCursorClient::EnableMouseEvents() {
  mouse_events_enabled_ = true;
}

void TestCursorClient::DisableMouseEvents() {
  mouse_events_enabled_ = false;
}

bool TestCursorClient::IsMouseEventsEnabled() const {
  return mouse_events_enabled_;
}

void TestCursorClient::SetDisplay(const display::Display& display) {}

const display::Display& TestCursorClient::GetDisplay() const {
  static const display::Display display;
  return display;
}

void TestCursorClient::LockCursor() {
  cursor_lock_count_++;
}

void TestCursorClient::UnlockCursor() {
  cursor_lock_count_--;
  if (cursor_lock_count_ < 0)
    cursor_lock_count_ = 0;
}

bool TestCursorClient::IsCursorLocked() const {
  return cursor_lock_count_ > 0;
}

void TestCursorClient::AddObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.AddObserver(observer);
}

void TestCursorClient::RemoveObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestCursorClient::ShouldHideCursorOnKeyEvent(
    const ui::KeyEvent& event) const {
  return should_hide_cursor_on_key_event_;
}

bool TestCursorClient::ShouldHideCursorOnTouchEvent(
    const ui::TouchEvent& event) const {
  return true;
}

gfx::Size TestCursorClient::GetSystemCursorSize() const {
  return gfx::Size(25, 25);
}

}  // namespace test
}  // namespace aura
