// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/buildflags.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"
#include "ui/views/views_export.h"

namespace aura::client {
class DragDropDelegate;
}  // namespace aura::client

namespace ui {
class DropTargetEvent;
}

namespace views {
class Widget;

class VIEWS_EXPORT DesktopDragDropClientOzone
    : public aura::client::DragDropClient,
      public ui::WmDropHandler,
      public aura::WindowObserver {
 public:
  DesktopDragDropClientOzone(aura::Window* root_window,
                             ui::WmDragHandler* drag_handler);

  DesktopDragDropClientOzone(const DesktopDragDropClientOzone&) = delete;
  DesktopDragDropClientOzone& operator=(const DesktopDragDropClientOzone&) =
      delete;

  ~DesktopDragDropClientOzone() override;

 protected:
  friend class DesktopDragDropClientOzoneTest;

  // Holds data related to the drag operation started by this client.
  struct DragContext {
    DragContext();
    ~DragContext();

    // Widget that the user drags around.  May be nullptr.
    std::unique_ptr<Widget> widget;

    // The size of drag image.
    gfx::Size size;

    // The offset of |drag_widget_| relative to the mouse position.
    gfx::Vector2d offset;

#if BUILDFLAG(IS_LINUX)
    // The last received drag location.  The drag widget is moved asynchronously
    // so its position is updated when the UI thread has time for that.  When
    // the first change to the location happens, a call to UpdateDragWidget()
    // is posted, and this location is set.  The location can be updated a few
    // more times until the posted task is executed, but no more than a single
    // call to UpdateDragWidget() is scheduled at any time; this optional is set
    // means that the task is scheduled.
    // This is used on a platform where chrome manages a drag image (e.g. x11).
    absl::optional<gfx::Point> last_screen_location_px;
#endif
  };

  // aura::client::DragDropClient
  ui::mojom::DragOperation StartDragAndDrop(
      std::unique_ptr<ui::OSExchangeData> data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& root_location,
      int allowed_operations,
      ui::mojom::DragEventSource source) override;
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override;
#endif
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override;
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override;

  // ui::WmDropHandler
  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<ui::OSExchangeData> data,
                   int operation,
                   int modifiers) override;
  int OnDragMotion(const gfx::PointF& point,
                   int operation,
                   int modifiers) override;
  void OnDragDrop(std::unique_ptr<ui::OSExchangeData> data,
                  int modifiers) override;
  void OnDragLeave() override;

  // aura::WindowObserver
  void OnWindowDestroyed(aura::Window* window) override;

  // Returns a WmDragHandler::LocationDelegate passed to `StartDrag`.
  virtual ui::WmDragHandler::LocationDelegate* GetLocationDelegate();

  void OnDragFinished(ui::mojom::DragOperation operation);

  // Returns a DropTargetEvent to be passed to the DragDropDelegate.
  // Updates the delegate if needed, which in its turn calls their
  // OnDragExited/OnDragEntered, so after getting the event the delegate
  // is ready to accept OnDragUpdated or GetDropCallback.  Returns nullptr if
  // drop is not possible.
  std::unique_ptr<ui::DropTargetEvent> UpdateTargetAndCreateDropEvent(
      const gfx::PointF& point,
      int modifiers);

  // Updates |drag_drop_delegate_| along with |window|.
  void UpdateDragDropDelegate(aura::Window* window);

  // Resets |drag_drop_delegate_|.
  // |send_exit| controls whether to call delegate's OnDragExited() before
  // resetting.
  void ResetDragDropTarget(bool send_exit);

  DragContext* drag_context() { return drag_context_.get(); }

  aura::Window* root_window() { return root_window_; }

 private:
  const raw_ptr<aura::Window> root_window_;

  const raw_ptr<ui::WmDragHandler> drag_handler_;

  aura::client::DragUpdateInfo current_drag_info_;

  // Last window under the mouse.
  raw_ptr<aura::Window> current_window_ = nullptr;
  // The delegate corresponding to the window located at the mouse position.
  raw_ptr<aura::client::DragDropDelegate> drag_drop_delegate_ = nullptr;

  // The data to be delivered through the drag and drop.
  std::unique_ptr<ui::OSExchangeData> data_to_drop_;

  // The most recent native coordinates of an incoming drag.  Updated while
  // the mouse is moved, and used at dropping.
  gfx::PointF last_drag_point_;
  // The most recent drop operation. Updated while the mouse is moved, and
  // used at dropping.
  int last_drop_operation_ = 0;

  // The selected operation on drop.
  ui::mojom::DragOperation drag_operation_ = ui::mojom::DragOperation::kNone;

  std::unique_ptr<DragContext> drag_context_;

  base::WeakPtrFactory<DesktopDragDropClientOzone> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_H_
