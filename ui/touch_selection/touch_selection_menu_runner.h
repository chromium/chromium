// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_MENU_RUNNER_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_MENU_RUNNER_H_

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "ui/touch_selection/ui_touch_selection_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
class Size;
}

namespace ui {

// Client interface for TouchSelectionMenuRunner.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionMenuClient {
 public:
  virtual ~TouchSelectionMenuClient() {}

  virtual bool IsCommandIdEnabled(int command_id) const = 0;
  virtual void ExecuteCommand(int command_id, int event_flags) = 0;

  // Called when the quick menu needs to run a context menu. Depending on the
  // implementation, this may run the context menu synchronously, or request the
  // menu to show up in which case the menu will run asynchronously at a later
  // time.
  virtual void RunContextMenu() = 0;

  // Whether the Quick Menu should be opened.
  virtual bool ShouldShowQuickMenu() = 0;

  // Returns the current text selection.
  virtual base::string16 GetSelectedText() = 0;
};

// An interface for the singleton object responsible for running touch selection
// quick menu.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionMenuRunner {
 public:
  virtual ~TouchSelectionMenuRunner();

  static TouchSelectionMenuRunner* GetInstance();

  // Checks whether there is any command available to show in the menu.
  virtual bool IsMenuAvailable(
      const TouchSelectionMenuClient* client) const = 0;

  // Creates and displays the quick menu, if there is any command available.
  // |anchor_rect| is in screen coordinates.
  virtual void OpenMenu(TouchSelectionMenuClient* client,
                        const gfx::Rect& anchor_rect,
                        const gfx::Size& handle_image_size,
                        aura::Window* context) = 0;

  virtual void CloseMenu() = 0;

  virtual bool IsRunning() const = 0;

 protected:
  TouchSelectionMenuRunner();

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunner);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_MENU_RUNNER_H_
