// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"

#include <fcntl.h>
#include <algorithm>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

WaylandDataOffer::WaylandDataOffer(wl_data_offer* data_offer)
    : data_offer_(data_offer),
      source_actions_(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE),
      dnd_action_(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) {
  static const struct wl_data_offer_listener kDataOfferListener = {
      WaylandDataOffer::OnOffer, WaylandDataOffer::OnSourceAction,
      WaylandDataOffer::OnAction};
  wl_data_offer_add_listener(data_offer, &kDataOfferListener, this);
}

WaylandDataOffer::~WaylandDataOffer() {
  data_offer_.reset();
}

void WaylandDataOffer::SetAction(uint32_t dnd_actions,
                                 uint32_t preferred_action) {
  if (wl_data_offer_get_version(data_offer_.get()) >=
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
    wl_data_offer_set_actions(data_offer_.get(), dnd_actions, preferred_action);
  }
}

void WaylandDataOffer::Accept(uint32_t serial, const std::string& mime_type) {
  wl_data_offer_accept(data_offer_.get(), serial, mime_type.c_str());
}

void WaylandDataOffer::Reject(uint32_t serial) {
  // Passing a null MIME type means "reject."
  wl_data_offer_accept(data_offer_.get(), serial, nullptr);
}

base::ScopedFD WaylandDataOffer::Receive(const std::string& mime_type) {
  if (!base::Contains(mime_types(), mime_type))
    return base::ScopedFD();

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  PCHECK(base::CreatePipe(&read_fd, &write_fd));

  // If we needed to forcibly write "text/plain" as an available
  // mimetype, then it is safer to "read" the clipboard data with
  // a mimetype mime_type known to be available.
  std::string effective_mime_type = mime_type;
  if (mime_type == kMimeTypeText && text_plain_mime_type_inserted())
    effective_mime_type = kMimeTypeTextUtf8;

  wl_data_offer_receive(data_offer_.get(), effective_mime_type.data(),
                        write_fd.get());
  return read_fd;
}

void WaylandDataOffer::FinishOffer() {
  if (wl_data_offer_get_version(data_offer_.get()) >=
      WL_DATA_OFFER_FINISH_SINCE_VERSION)
    wl_data_offer_finish(data_offer_.get());
}

uint32_t WaylandDataOffer::source_actions() const {
  return source_actions_;
}

uint32_t WaylandDataOffer::dnd_action() const {
  return dnd_action_;
}

// static
void WaylandDataOffer::OnOffer(void* data,
                               wl_data_offer* data_offer,
                               const char* mime_type) {
  auto* self = static_cast<WaylandDataOffer*>(data);
  self->AddMimeType(mime_type);
}

void WaylandDataOffer::OnSourceAction(void* data,
                                      wl_data_offer* offer,
                                      uint32_t source_actions) {
  auto* self = static_cast<WaylandDataOffer*>(data);
  self->source_actions_ = source_actions;
}

void WaylandDataOffer::OnAction(void* data,
                                wl_data_offer* offer,
                                uint32_t dnd_action) {
  auto* self = static_cast<WaylandDataOffer*>(data);
  self->dnd_action_ = dnd_action;
}

}  // namespace ui
