// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_

#include <wayland-client.h>

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/internal/wayland_data_offer_base.h"

namespace ui {

// This class represents a piece of data offered for transfer by another
// client, the source client (see WaylandDataSource for more).
// It is used by the copy-and-paste and drag-and-drop mechanisms.
//
// The offer describes the different mime types that the data can be
// converted to and provides the mechanism for transferring the data
// directly from the source client.
class WaylandDataOffer : public internal::WaylandDataOfferBase {
 public:
  // Takes ownership of data_offer.
  explicit WaylandDataOffer(wl_data_offer* data_offer);
  ~WaylandDataOffer() override;

  void SetAction(uint32_t dnd_actions, uint32_t preferred_action);
  void Accept(uint32_t serial, const std::string& mime_type);
  void Reject(uint32_t serial);

  // internal::WaylandDataOfferBase overrides:
  base::ScopedFD Receive(const std::string& mime_type) override;

  void FinishOffer();
  uint32_t source_actions() const;
  uint32_t dnd_action() const;

 private:
  // wl_data_offer_listener callbacks.
  static void OnOffer(void* data,
                      wl_data_offer* data_offer,
                      const char* mime_type);
  // Notifies the source-side available actions
  static void OnSourceAction(void* data,
                             wl_data_offer* offer,
                             uint32_t source_actions);
  // Notifies the selected action
  static void OnAction(void* data, wl_data_offer* offer, uint32_t dnd_action);

  wl::Object<wl_data_offer> data_offer_;
  // Actions offered by the data source
  uint32_t source_actions_;
  // Action selected by the compositor
  uint32_t dnd_action_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataOffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_
