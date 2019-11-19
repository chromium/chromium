// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace wm {

namespace internal {

// Represents the cursor state which is composed of cursor type, visibility, and
// mouse events enable state. When mouse events are disabled, the cursor is
// always invisible.
class CursorState {
 public:
  CursorState()
      : cursor_(ui::CursorType::kNone),
        visible_(true),
        cursor_size_(ui::CursorSize::kNormal),
        mouse_events_enabled_(true),
        visible_on_mouse_events_enabled_(true) {}

  gfx::NativeCursor cursor() const { return cursor_; }
  void set_cursor(gfx::NativeCursor cursor) { cursor_ = cursor; }

  bool visible() const { return visible_; }
  void SetVisible(bool visible) {
    if (mouse_events_enabled_)
      visible_ = visible;
    // Ignores the call when mouse events disabled.
  }

  ui::CursorSize cursor_size() const { return cursor_size_; }
  void set_cursor_size(ui::CursorSize cursor_size) {
    cursor_size_ = cursor_size;
  }

  bool mouse_events_enabled() const { return mouse_events_enabled_; }
  void SetMouseEventsEnabled(bool enabled) {
    if (mouse_events_enabled_ == enabled)
      return;
    mouse_events_enabled_ = enabled;

    // Restores the visibility when mouse events are enabled.
    if (enabled) {
      visible_ = visible_on_mouse_events_enabled_;
    } else {
      visible_on_mouse_events_enabled_ = visible_;
      visible_ = false;
    }
  }

 private:
  gfx::NativeCursor cursor_;
  bool visible_;
  ui::CursorSize cursor_size_;
  bool mouse_events_enabled_;

  // The visibility to set when mouse events are enabled.
  bool visible_on_mouse_events_enabled_;

  DISALLOW_COPY_AND_ASSIGN(CursorState);
};

}  // namespace internal

bool CursorManager::last_cursor_visibility_state_ = true;

CursorManager::CursorManager(std::unique_ptr<NativeCursorManager> delegate)
    : delegate_(std::move(delegate)),
      cursor_lock_count_(0),
      current_state_(new internal::CursorState),
      state_on_unlock_(new internal::CursorState) {
  // Restore the last cursor visibility state.
  current_state_->SetVisible(last_cursor_visibility_state_);
}

CursorManager::~CursorManager() {
}

// static
void CursorManager::ResetCursorVisibilityStateForTest() {
  last_cursor_visibility_state_ = true;
}

void CursorManager::SetCursor(gfx::NativeCursor cursor) {
  bool previously_visible = GetCursor().native_type() != ui::CursorType::kNone;
  state_on_unlock_->set_cursor(cursor);
  if (cursor_lock_count_ == 0 &&
      GetCursor() != state_on_unlock_->cursor()) {
    delegate_->SetCursor(state_on_unlock_->cursor(), this);
    bool is_visible = cursor.native_type() != ui::CursorType::kNone;
    if (is_visible != previously_visible) {
      for (auto& observer : observers_)
        observer.OnCursorVisibilityChanged(is_visible);
    }
  }
}

gfx::NativeCursor CursorManager::GetCursor() const {
  return current_state_->cursor();
}

void CursorManager::ShowCursor() {
  last_cursor_visibility_state_ = true;
  state_on_unlock_->SetVisible(true);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    if (GetCursor().native_type() != ui::CursorType::kNone) {
      // If the cursor is a visible type, notify the observers.
      for (auto& observer : observers_)
        observer.OnCursorVisibilityChanged(true);
    }
  }
}

void CursorManager::HideCursor() {
  last_cursor_visibility_state_ = false;
  state_on_unlock_->SetVisible(false);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    for (auto& observer : observers_)
      observer.OnCursorVisibilityChanged(false);
  }
}

bool CursorManager::IsCursorVisible() const {
  return current_state_->visible();
}

void CursorManager::SetCursorSize(ui::CursorSize cursor_size) {
  state_on_unlock_->set_cursor_size(cursor_size);
  if (GetCursorSize() != state_on_unlock_->cursor_size()) {
    delegate_->SetCursorSize(state_on_unlock_->cursor_size(), this);
    for (auto& observer : observers_)
      observer.OnCursorSizeChanged(cursor_size);
  }
}

ui::CursorSize CursorManager::GetCursorSize() const {
  return current_state_->cursor_size();
}

void CursorManager::EnableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(true);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
}

void CursorManager::DisableMouseEvents() {
  state_on_unlock_->SetMouseEventsEnabled(false);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
}

bool CursorManager::IsMouseEventsEnabled() const {
  return current_state_->mouse_events_enabled();
}

void CursorManager::SetDisplay(const display::Display& display) {
  display_ = display;
  for (auto& observer : observers_)
    observer.OnCursorDisplayChanged(display);

  delegate_->SetDisplay(display, this);
}

const display::Display& CursorManager::GetDisplay() const {
  return display_;
}

void CursorManager::LockCursor() {
  cursor_lock_count_++;
}

void CursorManager::UnlockCursor() {
  cursor_lock_count_--;
  DCHECK_GE(cursor_lock_count_, 0);
  if (cursor_lock_count_ > 0)
    return;

  if (GetCursor() != state_on_unlock_->cursor()) {
    delegate_->SetCursor(state_on_unlock_->cursor(), this);
  }
  if (IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
  if (IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(),
                             this);
  }
}

bool CursorManager::IsCursorLocked() const {
  return cursor_lock_count_ > 0;
}

void CursorManager::AddObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.AddObserver(observer);
}

void CursorManager::RemoveObserver(
    aura::client::CursorClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool CursorManager::ShouldHideCursorOnKeyEvent(
    const ui::KeyEvent& event) const {
  return false;
}

void CursorManager::CommitCursor(gfx::NativeCursor cursor) {
  current_state_->set_cursor(cursor);
}

void CursorManager::CommitVisibility(bool visible) {
  // TODO(tdanderson): Find a better place for this so we don't
  // notify the observers more than is necessary.
  for (auto& observer : observers_) {
    observer.OnCursorVisibilityChanged(
        GetCursor().native_type() == ui::CursorType::kNone ? false : visible);
  }
  current_state_->SetVisible(visible);
}

void CursorManager::CommitCursorSize(ui::CursorSize cursor_size) {
  current_state_->set_cursor_size(cursor_size);
}

void CursorManager::CommitMouseEventsEnabled(bool enabled) {
  current_state_->SetMouseEventsEnabled(enabled);
}

}  // namespace wm
