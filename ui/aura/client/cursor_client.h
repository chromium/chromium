// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_CURSOR_CLIENT_H_
#define UI_AURA_CLIENT_CURSOR_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace display {
class Display;
}

namespace gfx {
class Size;
}

namespace ui {
class KeyEvent;
class TouchEvent;
enum class CursorSize;
}

namespace aura {
class Window;
namespace client {
class CursorClientObserver;

// An interface that receives cursor change events.
class AURA_EXPORT CursorClient {
 public:
  // Notes that |window| has requested the change to |cursor|.
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;

  // Returns the current cursor.
  virtual gfx::NativeCursor GetCursor() const = 0;

  // Forces the cursor to be updated. This is called when the system may have
  // changed the cursor without the cursor client's knowledge, which breaks
  // if the cursor client doesn't think the cursor has changed.
  virtual void SetCursorForced(gfx::NativeCursor cursor) = 0;

  // Shows the cursor. This does not take effect When mouse events are disabled.
  virtual void ShowCursor() = 0;

  // Hides the cursor. Mouse events keep being sent even when the cursor is
  // invisible.
  virtual void HideCursor() = 0;

  // Sets the type of the mouse cursor icon.
  virtual void SetCursorSize(ui::CursorSize cursor_size) = 0;

  // Gets the type of the mouse cursor icon.
  virtual ui::CursorSize GetCursorSize() const = 0;

  // Gets whether the cursor is visible.
  virtual bool IsCursorVisible() const = 0;

  // Makes mouse events start being sent and shows the cursor if it was hidden
  // with DisableMouseEvents.
  virtual void EnableMouseEvents() = 0;

  // Makes mouse events stop being sent and hides the cursor if it is visible.
  virtual void DisableMouseEvents() = 0;

  // Returns true if mouse events are enabled.
  virtual bool IsMouseEventsEnabled() const = 0;

  // Sets the display for the cursor.
  virtual void SetDisplay(const display::Display& display) = 0;

  // Returns the display where the cursor is located.
  virtual const display::Display& GetDisplay() const = 0;

  // Locks the cursor change. The cursor type, cursor visibility, and mouse
  // events enable state never change as long as lock is held by anyone.
  virtual void LockCursor() = 0;

  // Unlocks the cursor change. If all the locks are released, the cursor type,
  // cursor visibility, and mouse events enable state are restored to the ones
  // set by the lastest call of SetCursor, ShowCursor/HideCursor, and
  // EnableMouseEvents/DisableMouseEvents.
  virtual void UnlockCursor() = 0;

  // Returns true if the cursor is locked.
  virtual bool IsCursorLocked() const = 0;

  // Used to add or remove a CursorClientObserver.
  virtual void AddObserver(CursorClientObserver* observer) = 0;
  virtual void RemoveObserver(CursorClientObserver* observer) = 0;

  // Returns true if the mouse cursor should be hidden on |event|.
  virtual bool ShouldHideCursorOnKeyEvent(const ui::KeyEvent& event) const = 0;
  virtual bool ShouldHideCursorOnTouchEvent(
      const ui::TouchEvent& event) const = 0;

  // Returns the OS cursor size in DIP.
  virtual gfx::Size GetSystemCursorSize() const = 0;

 protected:
  virtual ~CursorClient() {}
};

// Sets/Gets the activation client for the specified window.
AURA_EXPORT void SetCursorClient(Window* window,
                                 CursorClient* client);
AURA_EXPORT CursorClient* GetCursorClient(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_CURSOR_CLIENT_H_
