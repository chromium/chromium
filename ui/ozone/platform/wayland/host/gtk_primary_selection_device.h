// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_device_base.h"

struct gtk_primary_selection_device;

namespace ui {

class WaylandConnection;

// This class provides access to primary selection clipboard available on GTK.
class GtkPrimarySelectionDevice : public internal::WaylandDataDeviceBase {
 public:
  GtkPrimarySelectionDevice(WaylandConnection* connection,
                            gtk_primary_selection_device* data_device);
  ~GtkPrimarySelectionDevice() override;

  gtk_primary_selection_device* data_device() const {
    return data_device_.get();
  }

 private:
  // gtk_primary_selection_device_listener callbacks
  static void OnDataOffer(void* data,
                          gtk_primary_selection_device* data_device,
                          gtk_primary_selection_offer* offer);
  static void OnSelection(void* data,
                          gtk_primary_selection_device* data_device,
                          gtk_primary_selection_offer* offer);

  // The Wayland object wrapped by this instance.
  wl::Object<gtk_primary_selection_device> data_device_;

  DISALLOW_COPY_AND_ASSIGN(GtkPrimarySelectionDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_DEVICE_H_
