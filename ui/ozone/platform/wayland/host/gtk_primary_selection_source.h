// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_SOURCE_H_

#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_source_base.h"
#include "ui/ozone/public/platform_clipboard.h"

struct gtk_primary_selection_source;

namespace ui {

class WaylandConnection;

class GtkPrimarySelectionSource : public internal::WaylandDataSourceBase {
 public:
  // Takes ownership of data_source.
  GtkPrimarySelectionSource(gtk_primary_selection_source* data_source,
                            WaylandConnection* connection);
  ~GtkPrimarySelectionSource() override;

  void WriteToClipboard(const PlatformClipboard::DataMap& data_map) override;

 private:
  // gtk_primary_selection_source_listener callbacks
  static void OnSend(void* data,
                     gtk_primary_selection_source* source,
                     const char* mime_type,
                     int32_t fd);
  static void OnCancelled(void* data, gtk_primary_selection_source* source);

  // The gtk_primary_selection_source wrapped by this instance.
  wl::Object<gtk_primary_selection_source> data_source_;

  WaylandConnection* connection_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GtkPrimarySelectionSource);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_SOURCE_H_
