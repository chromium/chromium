// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_OFFER_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_offer_base.h"

struct gtk_primary_selection_offer;

namespace ui {

// This class represents a piece of data offered for transfer by another
// client, the source client (see GtkPrimarySelectionSource for more).
// It is used by the primary selection mechanism.
//
// The offer describes MIME types that the data can be converted to and provides
// the mechanism for transferring the data directly from the source client.
class GtkPrimarySelectionOffer : public internal::WaylandDataOfferBase {
 public:
  // Takes ownership of data_offer.
  explicit GtkPrimarySelectionOffer(gtk_primary_selection_offer* data_offer);
  ~GtkPrimarySelectionOffer() override;

  // internal::WaylandDataOfferBase overrides:
  base::ScopedFD Receive(const std::string& mime_type) override;

 private:
  // gtk_primary_selection_offer_listener callbacks.
  static void OnOffer(void* data,
                      gtk_primary_selection_offer* data_offer,
                      const char* mime_type);

  // The Wayland object wrapped by this instance.
  wl::Object<gtk_primary_selection_offer> data_offer_;

  DISALLOW_COPY_AND_ASSIGN(GtkPrimarySelectionOffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_PRIMARY_SELECTION_OFFER_H_
