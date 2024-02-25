// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_

#include <string>

#include "base/files/scoped_file.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"

namespace ui {

// This class represents a piece of data offered for transfer by another
// client, the source client (see WaylandDataSource for more).
// It is used by the copy-and-paste and drag-and-drop mechanisms.
//
// The offer describes the different mime types that the data can be
// converted to and provides the mechanism for transferring the data
// directly from the source client.
class WaylandDataOffer : public WaylandDataOfferBase {
 public:
  // Takes ownership of data_offer.
  explicit WaylandDataOffer(wl_data_offer* data_offer);

  WaylandDataOffer(const WaylandDataOffer&) = delete;
  WaylandDataOffer& operator=(const WaylandDataOffer&) = delete;

  ~WaylandDataOffer() override;

  void Accept(uint32_t serial, const std::string& mime_type);
  void Reject(uint32_t serial);
  void FinishOffer();

  // WaylandDataOfferBase overrides:
  base::ScopedFD Receive(const std::string& mime_type) override;

  uint32_t source_actions() const { return source_actions_; }
  uint32_t dnd_action() const { return dnd_action_; }
  void SetDndActions(uint32_t dnd_actions);
  uint32_t id() const { return data_offer_.id(); }

 private:
  // wl_data_offer_listener callbacks:
  static void OnOffer(void* data,
                      wl_data_offer* data_offer,
                      const char* mime_type);
  // Notifies the source-side available actions
  static void OnSourceActions(void* data,
                              wl_data_offer* offer,
                              uint32_t source_actions);
  // Notifies the selected action
  static void OnAction(void* data, wl_data_offer* offer, uint32_t dnd_action);

  wl::Object<wl_data_offer> data_offer_;
  // Actions offered by the data source
  uint32_t source_actions_;
  // Action selected by the compositor
  uint32_t dnd_action_;
  // Whether a MIME type was accepted.
  bool mime_type_accepted_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_OFFER_H_
