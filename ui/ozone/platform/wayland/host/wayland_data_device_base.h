// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_

#include <string>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class WaylandConnection;

// Implements high level (protocol-agnostic) interface to a Wayland data device.
class WaylandDataDeviceBase {
 public:
  class SelectionDelegate {
   public:
    virtual void OnSelectionOffer(WaylandDataOfferBase* offer) = 0;
    virtual void OnSelectionDataReceived(const std::string& mime_type,
                                         PlatformClipboard::Data contents) = 0;

   protected:
    virtual ~SelectionDelegate() = default;
  };

  explicit WaylandDataDeviceBase(WaylandConnection* connection);
  virtual ~WaylandDataDeviceBase();

  // Sets the delegate instance responsible for handling section events.
  void set_selection_delegate(SelectionDelegate* selection_delegate) {
    DCHECK(!selection_delegate_ || !selection_delegate);
    selection_delegate_ = selection_delegate;
  }

  // Returns MIME types given by the current data offer.
  const std::vector<std::string>& GetAvailableMimeTypes() const;

  // Extracts data of the specified MIME type from the data offer.
  bool RequestSelectionData(const std::string& mime_type);

 protected:
  WaylandConnection* connection() const { return connection_; }
  WaylandDataOfferBase* data_offer() { return data_offer_.get(); }
  void set_data_offer(std::unique_ptr<WaylandDataOfferBase> data_offer) {
    data_offer_ = std::move(data_offer);
  }

  // Resets the data offer.
  void ResetDataOffer();
  // Reads data of the requested MIME type from the data offer and gives it to
  // the clipboard linked to the Wayland connection.
  void ReadClipboardDataFromFD(base::ScopedFD fd, const std::string& mime_type);

  // Registers DeferredReadCallback as display sync callback listener, to
  // ensure there is no pending operation to be performed by the compositor,
  // otherwise read(..) could block awaiting data to be sent to pipe. It is
  // reset once it's called.
  void RegisterDeferredReadCallback();

  void RegisterDeferredReadClosure(base::OnceClosure closure);

  SelectionDelegate* selection_delegate() { return selection_delegate_; }

 private:
  // wl_callback_listener callback
  static void DeferredReadCallback(void* data,
                                   struct wl_callback* cb,
                                   uint32_t time);

  void DeferredReadCallbackInternal(struct wl_callback* cb, uint32_t time);

  SelectionDelegate* selection_delegate_ = nullptr;

  // Used to call out to WaylandConnection once clipboard data has been
  // successfully read.
  WaylandConnection* const connection_;

  // Offer that holds the most-recent clipboard selection, or null if no
  // clipboard data is available.
  std::unique_ptr<WaylandDataOfferBase> data_offer_;

  // Before blocking on read(), make sure server has written data on the pipe.
  base::OnceClosure deferred_read_closure_;
  wl::Object<wl_callback> deferred_read_callback_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceBase);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_
