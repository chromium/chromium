// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

WaylandDataOffer::WaylandDataOffer(wl_data_offer* data_offer)
    : data_offer_(data_offer),
      source_actions_(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE),
      dnd_action_(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) {
  static constexpr wl_data_offer_listener kDataOfferListener = {
      .offer = &OnOffer,
      .source_actions = &OnSourceActions,
      .action = &OnAction};
  wl_data_offer_add_listener(data_offer, &kDataOfferListener, this);
}

WaylandDataOffer::~WaylandDataOffer() {
  data_offer_.reset();
}

void WaylandDataOffer::Accept(uint32_t serial, const std::string& mime_type) {
  mime_type_accepted_ = true;
  wl_data_offer_accept(data_offer_.get(), serial, mime_type.c_str());
}

void WaylandDataOffer::Reject(uint32_t serial) {
  mime_type_accepted_ = false;
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
  if (wl::get_version_of_object(data_offer_.get()) >=
      WL_DATA_OFFER_FINISH_SINCE_VERSION) {
    // As per the spec it is illegal to call finish if no mimetype was accepted
    // or no action was received.
    if (mime_type_accepted_ && dnd_action_) {
      wl_data_offer_finish(data_offer_.get());
    }
  }
}

void WaylandDataOffer::SetDndActions(uint32_t dnd_actions) {
  if (wl::get_version_of_object(data_offer_.get()) <
      WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
    return;
  }

  // Determine preferred action based on the given |dnd_actions|, prioritizing
  // "copy" over "move", if both are set.
  uint32_t preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  else if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

  wl_data_offer_set_actions(data_offer_.get(), dnd_actions, preferred_action);

  // Some compositors might take too long to send the "action" event, so that we
  // never reset `dnd_action_` before the drop happens. However, calling finish
  // in that case still leads to a protocol error. To prevent that, perform the
  // reset now already. See also
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/202.
  if (dnd_actions == 0) {
    dnd_action_ = 0;
  }
}

// static
void WaylandDataOffer::OnOffer(void* data,
                               wl_data_offer* data_offer,
                               const char* mime_type) {
  auto* self = static_cast<WaylandDataOffer*>(data);
  self->AddMimeType(mime_type);
}

void WaylandDataOffer::OnSourceActions(void* data,
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
