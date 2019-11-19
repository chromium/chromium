// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_source.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_source_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"

namespace ui {

WaylandClipboard::WaylandClipboard(
    WaylandDataDeviceManager* data_device_manager,
    WaylandDataDevice* data_device,
    GtkPrimarySelectionDeviceManager* primary_selection_device_manager,
    GtkPrimarySelectionDevice* primary_selection_device)
    : data_device_manager_(data_device_manager),
      data_device_(data_device),
      primary_selection_device_manager_(primary_selection_device_manager),
      primary_selection_device_(primary_selection_device) {
  DCHECK(data_device_manager_);
  DCHECK(data_device_);
}

WaylandClipboard::~WaylandClipboard() = default;

void WaylandClipboard::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  internal::WaylandDataSourceBase* data_source = nullptr;
  if (buffer == ClipboardBuffer::kCopyPaste) {
    if (!clipboard_data_source_)
      clipboard_data_source_ = data_device_manager_->CreateSource();
    data_source = clipboard_data_source_.get();
  } else {
    if (!primary_selection_device_manager_) {
      std::move(callback).Run();
      return;
    }
    if (!primary_data_source_)
      primary_data_source_ = primary_selection_device_manager_->CreateSource();
    data_source = primary_data_source_.get();
  }

  DCHECK(data_source);
  data_source->WriteToClipboard(data_map);
  data_source->set_data_map(data_map);

  std::move(callback).Run();
}

void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  read_clipboard_closure_ = std::move(callback);
  DCHECK(data_map);
  data_map_ = data_map;
  if (buffer == ClipboardBuffer::kCopyPaste) {
    if (!data_device_->RequestSelectionData(mime_type))
      SetData({}, mime_type);
  } else {
    if (!primary_selection_device_->RequestSelectionData(mime_type))
      SetData({}, mime_type);
  }
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return !!clipboard_data_source_;
  else
    return !!primary_data_source_;
}

void WaylandClipboard::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  CHECK(update_sequence_cb_.is_null())
      << " The callback can be installed only once.";
  update_sequence_cb_ = std::move(cb);
}

void WaylandClipboard::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    std::move(callback).Run(data_device_->GetAvailableMimeTypes());
  } else {
    DCHECK(primary_selection_device_);
    std::move(callback).Run(primary_selection_device_->GetAvailableMimeTypes());
  }
}

void WaylandClipboard::DataSourceCancelled(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste) {
    DCHECK(clipboard_data_source_);
    SetData({}, {});
    clipboard_data_source_.reset();
  } else {
    DCHECK(primary_data_source_);
    SetData({}, {});
    primary_data_source_.reset();
  }
}

void WaylandClipboard::SetData(const std::vector<uint8_t>& contents,
                               const std::string& mime_type) {
  if (!data_map_)
    return;

  (*data_map_)[mime_type] = contents;

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
}

void WaylandClipboard::UpdateSequenceNumber(ClipboardBuffer buffer) {
  if (!update_sequence_cb_.is_null())
    update_sequence_cb_.Run(buffer);
}

}  // namespace ui
