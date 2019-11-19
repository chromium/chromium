// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_source.h"

#include <gtk-primary-selection-client-protocol.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

GtkPrimarySelectionSource::GtkPrimarySelectionSource(
    gtk_primary_selection_source* data_source,
    WaylandConnection* connection)
    : data_source_(data_source), connection_(connection) {
  DCHECK(connection_);
  DCHECK(data_source_);

  static const struct gtk_primary_selection_source_listener
      kDataSourceListener = {GtkPrimarySelectionSource::OnSend,
                             GtkPrimarySelectionSource::OnCancelled};
  gtk_primary_selection_source_add_listener(data_source_.get(),
                                            &kDataSourceListener, this);
}

GtkPrimarySelectionSource::~GtkPrimarySelectionSource() = default;

// static
void GtkPrimarySelectionSource::OnSend(void* data,
                                       gtk_primary_selection_source* source,
                                       const char* mime_type,
                                       int32_t fd) {
  GtkPrimarySelectionSource* self =
      static_cast<GtkPrimarySelectionSource*>(data);
  std::string contents;
  base::Optional<std::vector<uint8_t>> mime_data;
  self->GetClipboardData(mime_type, &mime_data);
  if (!mime_data.has_value() && strcmp(mime_type, kMimeTypeTextUtf8) == 0)
    self->GetClipboardData(kMimeTypeText, &mime_data);
  contents.assign(mime_data->begin(), mime_data->end());
  bool result =
      base::WriteFileDescriptor(fd, contents.data(), contents.length());
  DCHECK(result);
  close(fd);
}

// static
void GtkPrimarySelectionSource::OnCancelled(
    void* data,
    gtk_primary_selection_source* source) {
  GtkPrimarySelectionSource* self =
      static_cast<GtkPrimarySelectionSource*>(data);
  self->connection_->clipboard()->DataSourceCancelled(
      ClipboardBuffer::kSelection);
}

void GtkPrimarySelectionSource::WriteToClipboard(
    const PlatformClipboard::DataMap& data_map) {
  for (const auto& data : data_map) {
    gtk_primary_selection_source_offer(data_source_.get(), data.first.c_str());
    if (strcmp(data.first.c_str(), kMimeTypeText) == 0)
      gtk_primary_selection_source_offer(data_source_.get(), kMimeTypeTextUtf8);
  }

  gtk_primary_selection_device_set_selection(
      connection_->primary_selection_device(), data_source_.get(),
      connection_->serial());

  connection_->ScheduleFlush();
}

}  // namespace ui
