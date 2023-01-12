// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
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
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner(ClipboardBuffer buffer) override;
  void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback data_changed_callback) override;
  bool IsSelectionBufferAvailable() const override;

 private:
  friend class WaylandClipboardTest;
  // Get the wl::Clipboard instance owning a given |buffer|. Can return null in
  // case |buffer| is unsupported. E.g: primary selection is not available.
  wl::Clipboard* GetClipboard(ClipboardBuffer buffer);

  // WaylandConnection providing optional data device managers, e.g: gtk
  // primary selection.
  const raw_ptr<WaylandConnection> connection_;

  const std::unique_ptr<wl::Clipboard> copypaste_clipboard_;
  std::unique_ptr<wl::Clipboard> primary_selection_clipboard_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CLIPBOARD_H_
