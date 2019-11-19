// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class GtkPrimarySelectionDevice;
class GtkPrimarySelectionDeviceManager;
class GtkPrimarySelectionSource;
class WaylandDataDevice;
class WaylandDataDeviceManager;

// Handles clipboard operations.
//
// Owned by WaylandConnection, which provides a data device and a data device
// manager.
class WaylandClipboard : public PlatformClipboard {
 public:
  WaylandClipboard(
      WaylandDataDeviceManager* data_device_manager,
      WaylandDataDevice* data_device,
      GtkPrimarySelectionDeviceManager* primary_selection_device_manager,
      GtkPrimarySelectionDevice* primary_selection_device);
  ~WaylandClipboard() override;

  // PlatformClipboard.
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner(ClipboardBuffer buffer) override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

  void DataSourceCancelled(ClipboardBuffer buffer);
  void SetData(const std::vector<uint8_t>& contents,
               const std::string& mime_type);
  void UpdateSequenceNumber(ClipboardBuffer buffer);

 private:
  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  PlatformClipboard::DataMap* data_map_ = nullptr;

  // Notifies whenever clipboard sequence number is changed. Can be empty if not
  // set.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // Stores the callback to be invoked upon data reading from clipboard.
  PlatformClipboard::RequestDataClosure read_clipboard_closure_;

  std::unique_ptr<WaylandDataSource> clipboard_data_source_;
  std::unique_ptr<GtkPrimarySelectionSource> primary_data_source_;

  // These four instances are owned by the connection.
  WaylandDataDeviceManager* const data_device_manager_;
  WaylandDataDevice* const data_device_;
  GtkPrimarySelectionDeviceManager* const primary_selection_device_manager_;
  GtkPrimarySelectionDevice* const primary_selection_device_;

  DISALLOW_COPY_AND_ASSIGN(WaylandClipboard);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
