// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_WM_WM_DROP_HANDLER_H_
#define UI_PLATFORM_WINDOW_WM_WM_DROP_HANDLER_H_

#include <memory>

#include "base/component_export.h"

namespace gfx {
class PointF;
}

namespace ui {
class PlatformWindow;
class OSExchangeData;

class COMPONENT_EXPORT(WM) WmDropHandler {
 public:
  // Notifies that drag has entered the window.
  // |point| is in the coordinate space of the PlatformWindow in DIP.
  // |operations| contains bitmask of ui::DragDropTypes suggested by the source.
  // |modifiers| contains bitmask of ui::EventFlags that accompany the event.
  virtual void OnDragEnter(const gfx::PointF& point,
                           int operations,
                           int modifiers) = 0;

  // Notifies that the data advertised by the drag source was fully fetched,
  // which is delivered through |data| parameter. It must be called after
  // OnDragEnter and before OnDragLeave/OnDragDrop. Callers must also ensure
  // that this function is called every time the cursor re-enters a given
  // window, even in a single drag session.
  virtual void OnDragDataAvailable(std::unique_ptr<OSExchangeData> data) = 0;

  // Notifies that drag location has changed.
  // |point| is in the coordinate space of the PlatformWindow in DIP.
  // |operations| contains bitmask of ui::DragDropTypes suggested by the source.
  // |modifiers| contains bitmask of ui::EventFlags that accompany the event.
  // Returns one of ui::DragDropTypes values selected by the client.
  virtual int OnDragMotion(const gfx::PointF& point,
                           int operations,
                           int modifiers) = 0;

  // Notifies that the dragged data has been dropped. The location of the drop
  // is the location of the latest DragEnter/DragMotion. |modifiers| contains a
  // bitmask of ui::EventFlags that accompany the event.
  virtual void OnDragDrop(int modifiers) = 0;

  // Notifies that dragging is left. Must be called before
  // WmDragHandler::OnDragFinished when the drag session gets cancelled.
  virtual void OnDragLeave() = 0;

 protected:
  virtual ~WmDropHandler() = default;
};

COMPONENT_EXPORT(WM)
void SetWmDropHandler(PlatformWindow* platform_window,
                      WmDropHandler* drop_handler);
COMPONENT_EXPORT(WM)
WmDropHandler* GetWmDropHandler(const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_WM_WM_DROP_HANDLER_H_
