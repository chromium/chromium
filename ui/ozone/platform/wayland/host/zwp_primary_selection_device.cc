// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device.h"

#include <primary-selection-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_offer.h"

namespace ui {

// static
ZwpPrimarySelectionDevice::ZwpPrimarySelectionDevice(
    WaylandConnection* connection,
    zwp_primary_selection_device_v1* data_device)
    : WaylandDataDeviceBase(connection), data_device_(data_device) {
  static const struct zwp_primary_selection_device_v1_listener kListener = {
      ZwpPrimarySelectionDevice::OnDataOffer,
      ZwpPrimarySelectionDevice::OnSelection};
  zwp_primary_selection_device_v1_add_listener(data_device_.get(), &kListener,
                                               this);
}

ZwpPrimarySelectionDevice::~ZwpPrimarySelectionDevice() = default;

void ZwpPrimarySelectionDevice::SetSelectionSource(
    ZwpPrimarySelectionSource* source) {
  DCHECK(source);
  zwp_primary_selection_device_v1_set_selection(
      data_device_.get(), source->data_source(), connection()->serial());
  connection()->ScheduleFlush();
}

// static
void ZwpPrimarySelectionDevice::OnDataOffer(
    void* data,
    zwp_primary_selection_device_v1* data_device,
    zwp_primary_selection_offer_v1* offer) {
  auto* self = static_cast<ZwpPrimarySelectionDevice*>(data);
  DCHECK(self);
  self->set_data_offer(std::make_unique<ZwpPrimarySelectionOffer>(offer));
}

// static
void ZwpPrimarySelectionDevice::OnSelection(
    void* data,
    zwp_primary_selection_device_v1* data_device,
    zwp_primary_selection_offer_v1* offer) {
  auto* self = static_cast<ZwpPrimarySelectionDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer selection data available to be fetched.
  if (!offer) {
    self->ResetDataOffer();
  } else {
    DCHECK(self->data_offer());
    self->data_offer()->EnsureTextMimeTypeIfNeeded();
  }

  if (self->selection_delegate())
    self->selection_delegate()->OnSelectionOffer(self->data_offer());
}

}  // namespace ui
