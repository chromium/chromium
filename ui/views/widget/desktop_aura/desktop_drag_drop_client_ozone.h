// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_

#include <memory>

#include "base/callback.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"
#include "ui/views/views_export.h"

namespace aura {
namespace client {
class DragDropDelegate;
}
}  // namespace aura

namespace ui {
class DropTargetEvent;
}

namespace views {
class DesktopNativeCursorManager;

class VIEWS_EXPORT DesktopDragDropClientOzone
    : public aura::client::DragDropClient,
      public ui::WmDropHandler {
 public:
  DesktopDragDropClientOzone(aura::Window* root_window,
                             views::DesktopNativeCursorManager* cursor_manager,
                             ui::WmDragHandler* drag_handler);
  ~DesktopDragDropClientOzone() override;

  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& root_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  // Overridden from void ui::WmDropHandler:
  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<ui::OSExchangeData> data,
                   int operation) override;
  int OnDragMotion(const gfx::PointF& point, int operation) override;
  void OnDragDrop(std::unique_ptr<ui::OSExchangeData> data) override;
  void OnDragLeave() override;

  void OnDragSessionClosed(int operation);

 private:
  void DragDropSessionCompleted();
  void QuitRunLoop();

  // Returns a DropTargetEvent to be passed to the DragDropDelegate, or null to
  // abort the drag.
  std::unique_ptr<ui::DropTargetEvent> CreateDropTargetEvent(
      const gfx::PointF& point);

  // Updates |drag_drop_delegate_| along with |window|.
  void UpdateDragDropDelegate(aura::Window* window);

  // Resets |drag_drop_delegate_|.
  void ResetDragDropTarget();

  void PerformDrop();

  aura::Window* const root_window_;

  DesktopNativeCursorManager* cursor_manager_;

  ui::WmDragHandler* const drag_handler_;

  // The delegate corresponding to the window located at the mouse position.
  aura::client::DragDropDelegate* drag_drop_delegate_ = nullptr;

  // The data to be delivered through the drag and drop.
  std::unique_ptr<ui::OSExchangeData> os_exchange_data_ = nullptr;

  // The most recent native coordinates of a drag.
  gfx::PointF last_drag_point_;

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
