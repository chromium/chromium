// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_offer.h"

#include <primary-selection-unstable-v1-client-protocol.h>

#include <fcntl.h>
#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

ZwpPrimarySelectionOffer::ZwpPrimarySelectionOffer(
    zwp_primary_selection_offer_v1* data_offer)
    : data_offer_(data_offer) {
  static constexpr zwp_primary_selection_offer_v1_listener
      kPrimarySelectionOfferListener = {.offer = &OnOffer};
  zwp_primary_selection_offer_v1_add_listener(
      data_offer, &kPrimarySelectionOfferListener, this);
}

ZwpPrimarySelectionOffer::~ZwpPrimarySelectionOffer() {
  data_offer_.reset();
}

base::ScopedFD ZwpPrimarySelectionOffer::Receive(const std::string& mime_type) {
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

  zwp_primary_selection_offer_v1_receive(
      data_offer_.get(), effective_mime_type.data(), write_fd.get());
  return read_fd;
}

// static
void ZwpPrimarySelectionOffer::OnOffer(
    void* data,
    zwp_primary_selection_offer_v1* data_offer,
    const char* mime_type) {
  auto* self = static_cast<ZwpPrimarySelectionOffer*>(data);
  self->AddMimeType(mime_type);
}

}  // namespace ui
