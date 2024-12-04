// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CURSOR_MANAGER_H_
#define UI_WM_CORE_CURSOR_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace ui {
class KeyEvent;
class TouchEvent;
enum class CursorSize;
}

namespace wm {

namespace internal {
class CursorState;
}

class NativeCursorManager;

// This class receives requests to change cursor properties, as well as
// requests to queue any further changes until a later time. It sends changes
// to the NativeCursorManager, which communicates back to us when these changes
// were made through the NativeCursorManagerDelegate interface.
class COMPONENT_EXPORT(UI_WM) CursorManager
    : public aura::client::CursorClient,
      public NativeCursorManagerDelegate {
 public:
  explicit CursorManager(std::unique_ptr<NativeCursorManager> delegate);

  CursorManager(const CursorManager&) = delete;
  CursorManager& operator=(const CursorManager&) = delete;

  ~CursorManager() override;

  // Resets the last visibility state, etc. Currently only called by tests.
  static void ResetCursorVisibilityStateForTest();

  // Overridden from aura::client::CursorClient:
  void SetCursor(gfx::NativeCursor) override;
  gfx::NativeCursor GetCursor() const override;
  void SetCursorForced(gfx::NativeCursor) override;
  void ShowCursor() override;
  void HideCursor() override;
  bool IsCursorVisible() const override;
  void SetCursorSize(ui::CursorSize cursor_size) override;
  ui::CursorSize GetCursorSize() const override;
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
  // Overridden from NativeCursorManagerDelegate:
  void CommitCursor(gfx::NativeCursor cursor) override;
  void CommitVisibility(bool visible) override;
  void CommitCursorSize(ui::CursorSize cursor_size) override;
  void CommitMouseEventsEnabled(bool enabled) override;
  void CommitSystemCursorSize(const gfx::Size& cursor_size) override;

  void SetCursorImpl(gfx::NativeCursor cursor, bool forced);

  std::unique_ptr<NativeCursorManager> delegate_;

  // Display where the cursor is located.
  display::Display display_;

  // Number of times LockCursor() has been invoked without a corresponding
  // UnlockCursor().
  int cursor_lock_count_;

  // The current state of the cursor.
  std::unique_ptr<internal::CursorState> current_state_;

  // The cursor state to restore when the cursor is unlocked.
  std::unique_ptr<internal::CursorState> state_on_unlock_;

  base::ObserverList<aura::client::CursorClientObserver>::
      UncheckedAndDanglingUntriaged observers_;

  // This flag holds the cursor visibility state for the duration of the
  // process. Defaults to true. This flag helps ensure that when a
  // CursorManager instance is created it gets populated with the correct
  // cursor visibility state.
  static bool last_cursor_visibility_state_;
};

}  // namespace wm

#endif  // UI_WM_CORE_CURSOR_MANAGER_H_
