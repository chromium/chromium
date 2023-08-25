// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_

#include <cstdint>

#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

struct gtk_primary_selection_device;

namespace ui {

class WaylandConnection;

// This class provides access to primary selection clipboard available on GTK.
class GtkPrimarySelectionDevice : public WaylandDataDeviceBase {
 public:
  GtkPrimarySelectionDevice(WaylandConnection* connection,
                            gtk_primary_selection_device* data_device);

  GtkPrimarySelectionDevice(const GtkPrimarySelectionDevice&) = delete;
  GtkPrimarySelectionDevice& operator=(const GtkPrimarySelectionDevice&) =
      delete;

  ~GtkPrimarySelectionDevice() override;

  gtk_primary_selection_device* data_device() const {
    return data_device_.get();
  }

  void SetSelectionSource(GtkPrimarySelectionSource* source, uint32_t serial);

 private:
  // gtk_primary_selection_device_listener callbacks:
  static void OnDataOffer(void* data,
                          gtk_primary_selection_device* selection_device,
                          gtk_primary_selection_offer* offer);
  static void OnSelection(void* data,
                          gtk_primary_selection_device* selection_device,
                          gtk_primary_selection_offer* offer);

  // The Wayland object wrapped by this instance.
  wl::Object<gtk_primary_selection_device> data_device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_
