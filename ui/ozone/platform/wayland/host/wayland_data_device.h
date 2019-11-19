// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_

#include <wayland-client.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/public/platform_clipboard.h"

class SkBitmap;

namespace ui {

class OSExchangeData;
class WaylandDataOffer;
class WaylandConnection;
class WaylandWindow;

// This class provides access to inter-client data transfer mechanisms
// such as copy-and-paste and drag-and-drop mechanisms.
class WaylandDataDevice : public internal::WaylandDataDeviceBase {
 public:
  WaylandDataDevice(WaylandConnection* connection, wl_data_device* data_device);
  ~WaylandDataDevice() override;

  // Requests the data to the platform when Chromium gets drag-and-drop started
  // by others. Once reading the data from platform is done, |callback| should
  // be called with the data.
  void RequestDragData(
      const std::string& mime_type,
      base::OnceCallback<void(const PlatformClipboard::Data&)> callback);
  // Delivers the data owned by Chromium which initiates drag-and-drop. |buffer|
  // is an output parameter and it should be filled with the data corresponding
  // to mime_type.
  void DeliverDragData(const std::string& mime_type, std::string* buffer);
  // Starts drag with |data| to be delivered, |operation| supported by the
  // source side initiated the dragging.
  void StartDrag(wl_data_source* data_source, const ui::OSExchangeData& data);
  // Resets |source_data_| when the dragging is finished.
  void ResetSourceData();

  wl_data_device* data_device() const { return data_device_.get(); }

  bool IsDragEntered() { return drag_offer_ != nullptr; }

 private:
  void ReadDragDataFromFD(
      base::ScopedFD fd,
      base::OnceCallback<void(const PlatformClipboard::Data&)> callback);

  // If source_data_ is not set, data is being dragged from an external
  // application (non-chromium).
  bool IsDraggingExternalData() const { return !source_data_; }

  // If OnLeave event occurs while it's reading drag data, it defers handling
  // it. Once reading data is completed, it's handled.
  void HandleDeferredLeaveIfNeeded();

  // wl_data_device_listener callbacks
  static void OnDataOffer(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  static void OnEnter(void* data,
                      wl_data_device* data_device,
                      uint32_t serial,
                      wl_surface* surface,
                      wl_fixed_t x,
                      wl_fixed_t y,
                      wl_data_offer* offer);

  static void OnMotion(void* data,
                       struct wl_data_device* data_device,
                       uint32_t time,
                       wl_fixed_t x,
                       wl_fixed_t y);

  static void OnDrop(void* data, struct wl_data_device* data_device);

  static void OnLeave(void* data, struct wl_data_device* data_device);

  // Called by the compositor when the window gets pointer or keyboard focus,
  // or clipboard content changes behind the scenes.
  //
  // https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_data_device
  static void OnSelection(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  // Returns the drag icon bitmap and creates and wayland surface to draw it
  // on, if a valid drag image is present in |data|; otherwise returns null.
  const SkBitmap* PrepareDragIcon(const OSExchangeData& data);
  void DrawDragIcon(const SkBitmap* bitmap);

  void OnDragDataReceived(const PlatformClipboard::Data& contents);

  // HandleUnprocessedMimeTypes asynchronously request and read data for every
  // negotiated mime type, one after another (OnDragDataReceived calls back
  // HandleUnprocessedMimeTypes so it finish only when there's no more items in
  // unprocessed_mime_types_ vector). HandleReceivedData is called once the
  // process is finished.
  void HandleUnprocessedMimeTypes();
  void HandleReceivedData(std::unique_ptr<ui::OSExchangeData> received_data);
  // Returns the next MIME type to be received from the source process, or an
  // empty string if there are no more interesting MIME types left to process.
  std::string SelectNextMimeType();

  // Set drag operation decided by client.
  void SetOperation(const int operation);

  // The wl_data_device wrapped by this WaylandDataDevice.
  wl::Object<wl_data_device> data_device_;

  // There are two separate data offers at a time, the drag offer and the
  // selection offer, each with independent lifetimes. When we receive a new
  // offer, it is not immediately possible to know whether the new offer is the
  // drag offer or the selection offer. This variable is used to store ownership
  // of new data offers temporarily until its identity becomes known.
  std::unique_ptr<WaylandDataOffer> new_offer_;

  // Offer to receive data from another process via drag-and-drop, or null if no
  // drag-and-drop from another process is in progress.
  std::unique_ptr<WaylandDataOffer> drag_offer_;

  WaylandWindow* window_ = nullptr;

  bool is_handling_dropped_data_ = false;
  bool is_leaving_ = false;

  std::unique_ptr<WaylandShmBuffer> shm_buffer_ = nullptr;
  wl::Object<wl_surface> icon_surface_;

  // Mime types to be handled.
  std::list<std::string> unprocessed_mime_types_;

  // The data delivered from Wayland
  std::unique_ptr<ui::OSExchangeData> received_data_;

  // When dragging is started from Chromium, |source_data_| is forwarded to
  // Wayland when they are ready to get the data.
  std::unique_ptr<ui::OSExchangeData> source_data_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_
