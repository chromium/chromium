// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DROP_TARGET_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DROP_TARGET_WIN_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/drop_target_win.h"

namespace aura {
namespace client {
class DragDropDelegate;
}
}  // namespace aura

namespace ui {
class DropTargetEvent;
class OSExchangeData;
}  // namespace ui

namespace views {

// DesktopDropTargetWin takes care of managing drag and drop for
// DesktopWindowTreeHostWin. It converts Windows OLE drop messages into
// aura::client::DragDropDelegate calls.
class DesktopDropTargetWin : public ui::DropTargetWin,
                             public aura::WindowObserver {
 public:
  explicit DesktopDropTargetWin(aura::Window* root_window);
  ~DesktopDropTargetWin() override;

 private:
  // ui::DropTargetWin implementation:
  DWORD OnDragEnter(IDataObject* data_object,
                    DWORD key_state,
                    POINT position,
                    DWORD effect) override;
  DWORD OnDragOver(IDataObject* data_object,
                   DWORD key_state,
                   POINT position,
                   DWORD effect) override;
  void OnDragLeave(IDataObject* data_object) override;
  DWORD OnDrop(IDataObject* data_object,
               DWORD key_state,
               POINT position,
               DWORD effect) override;

  // aura::WindowObserver implementation:
  void OnWindowDestroyed(aura::Window* window) override;

  // Common functionality for the ui::DropTargetWin methods to translate from
  // COM data types to Aura ones.
  void Translate(IDataObject* data_object,
                 DWORD key_state,
                 POINT cursor_position,
                 DWORD effect,
                 std::unique_ptr<ui::OSExchangeData>* data,
                 std::unique_ptr<ui::DropTargetEvent>* event,
                 aura::client::DragDropDelegate** delegate);

  void NotifyDragLeave();

  // The root window associated with this drop target.
  aura::Window* root_window_;

  // The Aura window that is currently under the cursor. We need to manually
  // keep track of this because Windows will only call our drag enter method
  // once when the user enters the associated HWND. But inside that HWND there
  // could be multiple aura windows, so we need to generate drag enter events
  // for them.
  aura::Window* target_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDropTargetWin);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DROP_TARGET_WIN_H_
