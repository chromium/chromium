// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_OFFER_H_

#include <string>

#include "base/files/scoped_file.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"

struct zwp_primary_selection_offer_v1;

namespace ui {

// This class represents a piece of data offered for transfer by another client,
// the source client (see ZwpPrimarySelectionSource for more). It is used by the
// primary selection mechanism.
//
// The offer describes MIME types that the data can be converted to and provides
// the mechanism for transferring the data directly from the source client.
class ZwpPrimarySelectionOffer : public WaylandDataOfferBase {
 public:
  // Takes ownership of data_offer.
  explicit ZwpPrimarySelectionOffer(zwp_primary_selection_offer_v1* data_offer);

  ZwpPrimarySelectionOffer(const ZwpPrimarySelectionOffer &) = delete;
  ZwpPrimarySelectionOffer &operator =(const ZwpPrimarySelectionOffer &) = delete;

  ~ZwpPrimarySelectionOffer() override;

  // WaylandDataOfferBase overrides:
  base::ScopedFD Receive(const std::string& mime_type) override;

 private:
  // zwp_primary_selection_offer_listener callbacks:
  static void OnOffer(void* data,
                      zwp_primary_selection_offer_v1* data_offer,
                      const char* mime_type);

  // The Wayland object wrapped by this instance.
  wl::Object<zwp_primary_selection_offer_v1> data_offer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_ZWP_PRIMARY_SELECTION_OFFER_H_
