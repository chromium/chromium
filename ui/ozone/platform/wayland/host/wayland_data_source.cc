// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

#include "base/files/file_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandDataSource::WaylandDataSource(wl_data_source* data_source,
                                     WaylandConnection* connection)
    : data_source_(data_source), connection_(connection) {
  static const struct wl_data_source_listener kDataSourceListener = {
      WaylandDataSource::OnTarget,      WaylandDataSource::OnSend,
      WaylandDataSource::OnCancel,      WaylandDataSource::OnDnDDropPerformed,
      WaylandDataSource::OnDnDFinished, WaylandDataSource::OnAction};
  wl_data_source_add_listener(data_source, &kDataSourceListener, this);
}

WaylandDataSource::~WaylandDataSource() = default;

void WaylandDataSource::WriteToClipboard(
    const PlatformClipboard::DataMap& data_map) {
  for (const auto& data : data_map) {
    wl_data_source_offer(data_source_.get(), data.first.c_str());
    if (strcmp(data.first.c_str(), kMimeTypeText) == 0)
      wl_data_source_offer(data_source_.get(), kMimeTypeTextUtf8);
  }
  wl_data_device_set_selection(connection_->data_device(), data_source_.get(),
                               connection_->serial());

  connection_->ScheduleFlush();
}

void WaylandDataSource::Offer(const ui::OSExchangeData& data) {
  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland specs
  // don't say anything like that, but here we follow that common practice:
  // begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (data.HasFile()) {
    mime_types.push_back(kMimeTypeURIList);
  }
  if (data.HasURL(ui::OSExchangeData::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    mime_types.push_back(kMimeTypeMozillaURL);
  }
  if (data.HasHtml()) {
    mime_types.push_back(kMimeTypeHTML);
  }
  if (data.HasString()) {
    mime_types.push_back(kMimeTypeTextUtf8);
    mime_types.push_back(kMimeTypeText);
  }

  source_window_ =
      connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  for (auto& mime_type : mime_types)
    wl_data_source_offer(data_source_.get(), mime_type.data());
}

void WaylandDataSource::SetDragData(const DragDataMap& data_map) {
  DCHECK(drag_data_map_.empty());
  drag_data_map_ = data_map;
}

void WaylandDataSource::SetAction(int operation) {
  if (wl_data_source_get_version(data_source_.get()) >=
      WL_DATA_SOURCE_SET_ACTIONS_SINCE_VERSION) {
    uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (operation & DragDropTypes::DRAG_COPY)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    if (operation & DragDropTypes::DRAG_MOVE)
      dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    wl_data_source_set_actions(data_source_.get(), dnd_actions);
  }
}

// static
void WaylandDataSource::OnTarget(void* data,
                                 wl_data_source* source,
                                 const char* mime_type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandDataSource::OnSend(void* data,
                               wl_data_source* source,
                               const char* mime_type,
                               int32_t fd) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  std::string contents;
  if (self->source_window_) {
    // If |source_window_| is valid when OnSend() is called, it means that DnD
    // is working.
    self->GetDragData(mime_type, &contents);
  } else {
    base::Optional<std::vector<uint8_t>> mime_data;
    self->GetClipboardData(mime_type, &mime_data);
    if (!mime_data.has_value() && strcmp(mime_type, kMimeTypeTextUtf8) == 0)
      self->GetClipboardData(kMimeTypeText, &mime_data);
    contents.assign(mime_data->begin(), mime_data->end());
  }
  bool result =
      base::WriteFileDescriptor(fd, contents.data(), contents.length());
  DCHECK(result);
  close(fd);
}

// static
void WaylandDataSource::OnCancel(void* data, wl_data_source* source) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  if (self->source_window_) {
    // If it has |source_window_|, it is in the middle of 'drag and drop'. it
    // cancels 'drag and drop'.
    self->connection_->FinishDragSession(self->dnd_action_,
                                         self->source_window_);
  } else {
    self->connection_->clipboard()->DataSourceCancelled(
        ClipboardBuffer::kCopyPaste);
  }
}

void WaylandDataSource::OnDnDDropPerformed(void* data, wl_data_source* source) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandDataSource::OnDnDFinished(void* data, wl_data_source* source) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  self->connection_->FinishDragSession(self->dnd_action_, self->source_window_);
}

void WaylandDataSource::OnAction(void* data,
                                 wl_data_source* source,
                                 uint32_t dnd_action) {
  WaylandDataSource* self = static_cast<WaylandDataSource*>(data);
  self->dnd_action_ = dnd_action;
}

void WaylandDataSource::GetDragData(const std::string& mime_type,
                                    std::string* contents) {
  auto it = drag_data_map_.find(mime_type);
  if (it != drag_data_map_.end()) {
    *contents = it->second;
    return;
  }

  connection_->DeliverDragData(mime_type, contents);
}

}  // namespace ui
