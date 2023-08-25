// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"

#include <gtk-primary-selection-client-protocol.h>

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

// static
GtkPrimarySelectionDevice::GtkPrimarySelectionDevice(
    WaylandConnection* connection,
    gtk_primary_selection_device* data_device)
    : WaylandDataDeviceBase(connection), data_device_(data_device) {
  static constexpr gtk_primary_selection_device_listener
      kPrimarySelectionListener = {.data_offer = &OnDataOffer,
                                   .selection = &OnSelection};
  gtk_primary_selection_device_add_listener(data_device_.get(),
                                            &kPrimarySelectionListener, this);
}

GtkPrimarySelectionDevice::~GtkPrimarySelectionDevice() = default;

void GtkPrimarySelectionDevice::SetSelectionSource(
    GtkPrimarySelectionSource* source,
    uint32_t serial) {
  auto* data_source = source ? source->data_source() : nullptr;
  gtk_primary_selection_device_set_selection(data_device_.get(), data_source,
                                             serial);
  connection()->Flush();
}

// static
void GtkPrimarySelectionDevice::OnDataOffer(
    void* data,
    gtk_primary_selection_device* selection_device,
    gtk_primary_selection_offer* offer) {
  auto* self = static_cast<GtkPrimarySelectionDevice*>(data);
  DCHECK(self);
  self->set_data_offer(std::make_unique<GtkPrimarySelectionOffer>(offer));
}

// static
void GtkPrimarySelectionDevice::OnSelection(
    void* data,
    gtk_primary_selection_device* selection_device,
    gtk_primary_selection_offer* offer) {
  auto* self = static_cast<GtkPrimarySelectionDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer selection data available to be fetched.
  if (!offer) {
    self->ResetDataOffer();
  } else {
    DCHECK(self->data_offer());
    self->data_offer()->EnsureTextMimeTypeIfNeeded();
  }

  self->NotifySelectionOffer(self->data_offer());
}

}  // namespace ui
