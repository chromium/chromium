// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace wl {
class Clipboard;
}  // namespace wl

namespace ui {

class WaylandConnection;
class WaylandDataDeviceManager;

// This class is a wrapper around Wayland data_device protocols that simulates
// typical clipboard operations. Unlike some other platforms, data-transfer is
// an async, lazy operation. This means that even after "writing" data to the
// system clipboard, this class must still hold on to a local cache of the
// clipboard contents, since it may be read (repeatedly) by other Wayland
// clients.
//
// WaylandDataDeviceManager singleton is required to be up and running for
// WaylandClipboard to be minimally functional.
class WaylandClipboard : public PlatformClipboard {
 public:
  WaylandClipboard(WaylandConnection* connection,
                   WaylandDataDeviceManager* device_manager);
  WaylandClipboard(const WaylandClipboard&) = delete;
  WaylandClipboard& operator=(const WaylandClipboard&) = delete;
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
  bool IsSelectionBufferAvailable() const override;

  // TODO(nickdiego): Get rid of these methods once DataDevice implementations
  // are decoupled from WaylandClipboard.
  void SetData(PlatformClipboard::Data contents, const std::string& mime_type);
  void UpdateSequenceNumber(ClipboardBuffer buffer);

 private:
  // Get the wl::Clipboard instance owning a given |buffer|. Can return null in
  // case |buffer| is unsupported. E.g: primary selection is not available.
  wl::Clipboard* GetClipboard(ClipboardBuffer buffer);

  // WaylandConnection providing optional data device managers, e.g: gtk
  // primary selection.
  WaylandConnection* const connection_;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  PlatformClipboard::DataMap* data_map_ = nullptr;

  // Notifies whenever clipboard sequence number is changed. Can be empty if
  // not set.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // Stores the callback to be invoked upon data reading from clipboard.
  PlatformClipboard::RequestDataClosure read_clipboard_closure_;

  const std::unique_ptr<wl::Clipboard> copypaste_clipboard_;
  std::unique_ptr<wl::Clipboard> primary_selection_clipboard_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
