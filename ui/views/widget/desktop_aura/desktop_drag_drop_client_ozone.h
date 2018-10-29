// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_

#include "base/callback.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/views/views_export.h"

namespace views {
class DesktopNativeCursorManager;

class VIEWS_EXPORT DesktopDragDropClientOzone
    : public aura::client::DragDropClient {
 public:
  DesktopDragDropClientOzone(aura::Window* root_window,
                             views::DesktopNativeCursorManager* cursor_manager,
                             ui::WmDragHandler* drag_handler);
  ~DesktopDragDropClientOzone() override;

  void OnDragSessionClosed(int operation);

  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& root_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

 private:
  void DragDropSessionCompleted();
  void QuitRunLoop();

  aura::Window* const root_window_;

  DesktopNativeCursorManager* cursor_manager_;

  ui::WmDragHandler* const drag_handler_;

  // Cursor in use prior to the move loop starting. Restored when the move loop
  // quits.
  gfx::NativeCursor initial_cursor_;

  base::OnceClosure quit_closure_;

  // The operation bitfield.
  int drag_operation_ = 0;

  //  The flag that controls whether it has a nested run loop.
  bool in_move_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientOzone);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
