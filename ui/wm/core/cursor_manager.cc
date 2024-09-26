// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
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
      : visible_(true),
        cursor_size_(ui::CursorSize::kNormal),
        mouse_events_enabled_(true),
        visible_on_mouse_events_enabled_(true) {}

  CursorState(const CursorState&) = delete;
  CursorState& operator=(const CursorState&) = delete;

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

  const gfx::Size& system_cursor_size() const { return system_cursor_size_; }
  void set_system_cursor_size(const gfx::Size& system_cursor_size) {
    system_cursor_size_ = system_cursor_size;
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

  gfx::Size system_cursor_size_;
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
  SetCursorImpl(cursor, /*forced=*/false);
}

gfx::NativeCursor CursorManager::GetCursor() const {
  return current_state_->cursor();
}

void CursorManager::SetCursorForced(gfx::NativeCursor cursor) {
  SetCursorImpl(cursor, /*forced=*/true);
}

void CursorManager::ShowCursor() {
  last_cursor_visibility_state_ = true;
  state_on_unlock_->SetVisible(true);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    if (GetCursor().type() != ui::mojom::CursorType::kNone) {
      // If the cursor is a visible type, notify the observers.
      observers_.Notify(
          &aura::client::CursorClientObserver::OnCursorVisibilityChanged, true);
    }
  }
}

void CursorManager::HideCursor() {
  last_cursor_visibility_state_ = false;
  state_on_unlock_->SetVisible(false);
  if (cursor_lock_count_ == 0 &&
      IsCursorVisible() != state_on_unlock_->visible()) {
    delegate_->SetVisibility(state_on_unlock_->visible(), this);
    observers_.Notify(
        &aura::client::CursorClientObserver::OnCursorVisibilityChanged, false);
  }
}

bool CursorManager::IsCursorVisible() const {
  return current_state_->visible();
}

void CursorManager::SetCursorSize(ui::CursorSize cursor_size) {
  state_on_unlock_->set_cursor_size(cursor_size);
  if (GetCursorSize() != state_on_unlock_->cursor_size()) {
    delegate_->SetCursorSize(state_on_unlock_->cursor_size(), this);
    observers_.Notify(&aura::client::CursorClientObserver::OnCursorSizeChanged,
                      cursor_size);
  }
}

ui::CursorSize CursorManager::GetCursorSize() const {
  return current_state_->cursor_size();
}

void CursorManager::EnableMouseEvents() {
  TRACE_EVENT0("ui,input", "CursorManager::EnableMouseEvents");
  state_on_unlock_->SetMouseEventsEnabled(true);
  if (cursor_lock_count_ == 0 &&
      IsMouseEventsEnabled() != state_on_unlock_->mouse_events_enabled()) {
    delegate_->SetMouseEventsEnabled(state_on_unlock_->mouse_events_enabled(),
                                     this);
  }
}

void CursorManager::DisableMouseEvents() {
  TRACE_EVENT0("ui,input", "CursorManager::DisableMouseEvents");
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
  observers_.Notify(&aura::client::CursorClientObserver::OnCursorDisplayChanged,
                    display);

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

bool CursorManager::ShouldHideCursorOnTouchEvent(
    const ui::TouchEvent& event) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  // Linux Aura does not hide the cursor on touch by default.
  // TODO(tdanderson): Change this if having consistency across
  // all platforms which use Aura is desired.
  return false;
#endif
}

void CursorManager::CommitCursor(gfx::NativeCursor cursor) {
  current_state_->set_cursor(cursor);
}

void CursorManager::CommitVisibility(bool visible) {
  // TODO(tdanderson): Find a better place for this so we don't
  // notify the observers more than is necessary.
  observers_.Notify(
      &aura::client::CursorClientObserver::OnCursorVisibilityChanged,
      GetCursor().type() == ui::mojom::CursorType::kNone ? false : visible);
  current_state_->SetVisible(visible);
}

void CursorManager::CommitCursorSize(ui::CursorSize cursor_size) {
  current_state_->set_cursor_size(cursor_size);
}

void CursorManager::CommitMouseEventsEnabled(bool enabled) {
  current_state_->SetMouseEventsEnabled(enabled);
}

gfx::Size CursorManager::GetSystemCursorSize() const {
  return current_state_->system_cursor_size();
}

void CursorManager::CommitSystemCursorSize(
    const gfx::Size& system_cursor_size) {
  current_state_->set_system_cursor_size(system_cursor_size);
  observers_.Notify(
      &aura::client::CursorClientObserver::OnSystemCursorSizeChanged,
      system_cursor_size);
}

void CursorManager::SetCursorImpl(gfx::NativeCursor cursor, bool forced) {
  bool previously_visible = GetCursor().type() != ui::mojom::CursorType::kNone;
  state_on_unlock_->set_cursor(cursor);
  if (cursor_lock_count_ == 0 &&
      (forced || GetCursor() != state_on_unlock_->cursor())) {
    delegate_->SetCursor(state_on_unlock_->cursor(), this);
    bool is_visible = cursor.type() != ui::mojom::CursorType::kNone;
    if (is_visible != previously_visible) {
      observers_.Notify(
          &aura::client::CursorClientObserver::OnCursorVisibilityChanged,
          is_visible);
    }
  }
}

}  // namespace wm
