// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DRAG_CONTROLLER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DRAG_CONTROLLER_H_

#include <list>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

struct wl_surface;
class SkBitmap;

namespace ui {

class OSExchangeData;
class WaylandConnection;
class WaylandDataDeviceManager;
class WaylandDataOffer;
class WaylandWindow;
class WaylandWindowManager;
class WaylandShmBuffer;

// WaylandDataDragController implements regular data exchange on top of the
// Wayland Drag and Drop protocol.  The data can be dragged within the Chromium
// window, or between Chromium and other application in both directions.
//
// The outgoing drag starts via the StartSession() method.  For more context,
// see WaylandTopLevelWindow::StartDrag().
//
// The incoming drag starts with the call to OnDragEnter() from the Wayland side
// (the data device), and ends up in call to WaylandWindow::OnDragEnter(), but
// two ways of coming there are possible:
//
// 1.  The drag has been initiated by a Chromium window.  In this case, the data
// that is being dragged is available right away, and therefore the controller
// can forward the data to the window immediately.
//
// 2.  The data is being dragged from another application.  Before notifying the
// window, the controller requests the data from the source side, which results
// in a number of requests to Wayland and data transfers from it.  Only after
// data records of all supported MIME types have been received, the window will
// be notified.
//
// It is possible that further drag events come while the data is still being
// transferred.  The drag motion event is ignored; the window will first receive
// OnDragEnter, and any OnDragMotion that comes after that.  The drag leave
// event stops the transfer and cancels the operation; the window will not
// receive anything at all.
class WaylandDataDragController : public WaylandDataDevice::DragDelegate,
                                  public WaylandDataSource::Delegate,
                                  public WaylandWindowObserver {
 public:
  enum class State {
    kIdle,          // Doing nothing special
    kStarted,       // The outgoing drag is in progress.
    kTransferring,  // The incoming data is transferred from the source.
  };

  WaylandDataDragController(WaylandConnection* connection,
                            WaylandDataDeviceManager* data_device_manager);
  WaylandDataDragController(const WaylandDataDragController&) = delete;
  WaylandDataDragController& operator=(const WaylandDataDragController&) =
      delete;
  ~WaylandDataDragController() override;

  // Starts a data drag and drop session for |data|. Only one DND session can
  // run at a given time.
  void StartSession(const ui::OSExchangeData& data, int operation);

  State state() const { return state_; }

  // TODO(crbug.com/896640): Remove once focus is fixed during DND sessions.
  WaylandWindow* entered_window() const { return window_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, ReceiveDrag);
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, StartDrag);
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, StartDragWithText);
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest,
                           StartDragWithWrongMimeType);

  // WaylandDataDevice::DragDelegate:
  bool IsDragSource() const override;
  void DrawIcon() override;
  void OnDragOffer(std::unique_ptr<WaylandDataOffer> offer) override;
  void OnDragEnter(WaylandWindow* window,
                   const gfx::PointF& location,
                   uint32_t serial) override;
  void OnDragMotion(const gfx::PointF& location) override;
  void OnDragLeave() override;
  void OnDragDrop() override;

  // WaylandDataSource::Delegate:
  void OnDataSourceFinish(bool completed) override;
  void OnDataSourceSend(const std::string& mime_type,
                        std::string* contents) override;

  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  void Offer(const OSExchangeData& data, int operation);
  void HandleUnprocessedMimeTypes(base::TimeTicks start_time);
  void OnMimeTypeDataTransferred(base::TimeTicks start_time,
                                 PlatformClipboard::Data contents);
  void OnDataTransferFinished(
      base::TimeTicks start_time,
      std::unique_ptr<ui::OSExchangeData> received_data);
  std::string GetNextUnprocessedMimeType();
  // Calls the window's OnDragEnter with the given location and data,
  // then immediately calls OnDragMotion to get the actual operation.
  void PropagateOnDragEnter(const gfx::PointF& location,
                            std::unique_ptr<OSExchangeData> data);

  WaylandConnection* const connection_;
  WaylandDataDeviceManager* const data_device_manager_;
  WaylandDataDevice* const data_device_;
  WaylandWindowManager* const window_manager_;

  State state_ = State::kIdle;

  // Data offered by us to the other side.
  std::unique_ptr<WaylandDataSource> data_source_;

  // When dragging is started from Chromium, |data_| holds the data to be sent
  // through wl_data_device instance.
  std::unique_ptr<ui::OSExchangeData> data_;

  // Offer to receive data from another process via drag-and-drop, or null if
  // no drag-and-drop from another process is in progress.
  //
  // The data offer from another Wayland client through wl_data_device, that
  // triggered the current drag and drop session. If null, either there is no
  // dnd session running or Chromium is the data source.
  std::unique_ptr<WaylandDataOffer> data_offer_;

  // Mime types to be handled.
  std::list<std::string> unprocessed_mime_types_;

  // The window that initiated the drag session. Can be null when the session
  // has been started by an external Wayland client.
  WaylandWindow* origin_window_ = nullptr;

  // Current window under pointer.
  WaylandWindow* window_ = nullptr;

  // The most recent location received while dragging the data.
  gfx::PointF last_drag_location_;

  // The data delivered from Wayland
  std::unique_ptr<ui::OSExchangeData> received_data_;

  // Set when 'leave' event is fired while data is being transferred.
  bool is_leave_pending_ = false;

  // Drag icon related variables.
  wl::Object<wl_surface> icon_surface_;
  std::unique_ptr<WaylandShmBuffer> shm_buffer_;
  const SkBitmap* icon_bitmap_ = nullptr;

  base::WeakPtrFactory<WaylandDataDragController> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DRAG_CONTROLLER_H_
