// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_LINUX_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

namespace views {

class VIEWS_EXPORT DesktopDragDropClientOzoneLinux
    : public DesktopDragDropClientOzone,
      public ui::WmDragHandler::LocationDelegate {
 public:
  DesktopDragDropClientOzoneLinux(aura::Window* root_window,
                                  ui::WmDragHandler* drag_handler);

  DesktopDragDropClientOzoneLinux(const DesktopDragDropClientOzoneLinux&) =
      delete;
  DesktopDragDropClientOzoneLinux& operator=(
      const DesktopDragDropClientOzoneLinux&) = delete;

  ~DesktopDragDropClientOzoneLinux() override;

 private:
  // DesktopdragDropClientOzone::
  ui::WmDragHandler::LocationDelegate* GetLocationDelegate() override;

  // ui::WmDragHandler::LocationDelegate:
  void OnDragLocationChanged(const gfx::Point& screen_point_px) override;
  void OnDragOperationChanged(ui::mojom::DragOperation operation) override;
  std::optional<gfx::AcceleratedWidget> GetDragWidget() override;

  // Updates |drag_widget_| so it is aligned with the last drag location.
  void UpdateDragWidgetLocation();

  base::WeakPtrFactory<DesktopDragDropClientOzoneLinux> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_OZONE_LINUX_H_
