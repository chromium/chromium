// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_H_

#include <cstdint>

#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

struct zwp_primary_selection_device_v1;

namespace ui {

class WaylandConnection;

// This class provides access to primary selection clipboard
class ZwpPrimarySelectionDevice : public WaylandDataDeviceBase {
 public:
  ZwpPrimarySelectionDevice(WaylandConnection* connection,
                            zwp_primary_selection_device_v1* data_device);

  ZwpPrimarySelectionDevice(const ZwpPrimarySelectionDevice&) = delete;
  ZwpPrimarySelectionDevice& operator=(const ZwpPrimarySelectionDevice&) =
      delete;

  ~ZwpPrimarySelectionDevice() override;

  zwp_primary_selection_device_v1* data_device() const {
    return data_device_.get();
  }

  void SetSelectionSource(ZwpPrimarySelectionSource* source, uint32_t serial);

 private:
  // zwp_primary_selection_device_listener callbacks:
  static void OnDataOffer(void* data,
                          zwp_primary_selection_device_v1* selection_device,
                          zwp_primary_selection_offer_v1* offer);
  static void OnSelection(void* data,
                          zwp_primary_selection_device_v1* selection_device,
                          zwp_primary_selection_offer_v1* offer);

  // The Wayland object wrapped by this instance.
  wl::Object<zwp_primary_selection_device_v1> data_device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_DEVICE_H_
