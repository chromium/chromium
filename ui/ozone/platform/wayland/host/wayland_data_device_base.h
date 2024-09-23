// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_

#include <string>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class WaylandConnection;

// Implements high level (protocol-agnostic) interface to a Wayland data device.
class WaylandDataDeviceBase {
 public:
  using SelectionOfferCallback =
      base::RepeatingCallback<void(WaylandDataOfferBase*)>;

  explicit WaylandDataDeviceBase(WaylandConnection* connection);

  WaylandDataDeviceBase(const WaylandDataDeviceBase&) = delete;
  WaylandDataDeviceBase& operator=(const WaylandDataDeviceBase&) = delete;

  virtual ~WaylandDataDeviceBase();

  // Sets the callback responsible for handling selection events.
  void set_selection_offer_callback(SelectionOfferCallback callback) {
    DCHECK(!selection_offer_callback_ || !callback);
    selection_offer_callback_ = callback;
  }

  // Returns MIME types given by the current data offer.
  const std::vector<std::string>& GetAvailableMimeTypes() const;

  // Synchronously reads and returns selection data with |mime_type| format.
  // TODO(crbug.com/40398800): Drop once Clipboard API becomes async.
  PlatformClipboard::Data ReadSelectionData(const std::string& mime_type);

 protected:
  WaylandConnection* connection() const { return connection_; }
  WaylandDataOfferBase* data_offer() { return data_offer_.get(); }
  void set_data_offer(std::unique_ptr<WaylandDataOfferBase> data_offer) {
    data_offer_ = std::move(data_offer);
  }

  // Resets the data offer.
  void ResetDataOffer();

  void NotifySelectionOffer(WaylandDataOfferBase* offer) const;

 private:
  SelectionOfferCallback selection_offer_callback_;

  // Used to call out to WaylandConnection once clipboard data has been
  // successfully read.
  const raw_ptr<WaylandConnection> connection_;

  // Offer that holds the most-recent clipboard selection, or null if no
  // clipboard data is available.
  std::unique_ptr<WaylandDataOfferBase> data_offer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_BASE_H_
