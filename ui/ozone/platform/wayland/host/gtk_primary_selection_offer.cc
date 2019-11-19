// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_offer.h"

#include <gtk-primary-selection-client-protocol.h>

#include <fcntl.h>
#include <algorithm>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

GtkPrimarySelectionOffer::GtkPrimarySelectionOffer(
    gtk_primary_selection_offer* data_offer)
    : data_offer_(data_offer) {
  static const struct gtk_primary_selection_offer_listener kListener = {
      GtkPrimarySelectionOffer::OnOffer};
  gtk_primary_selection_offer_add_listener(data_offer, &kListener, this);
}

GtkPrimarySelectionOffer::~GtkPrimarySelectionOffer() {
  data_offer_.reset();
}

base::ScopedFD GtkPrimarySelectionOffer::Receive(const std::string& mime_type) {
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

  gtk_primary_selection_offer_receive(
      data_offer_.get(), effective_mime_type.data(), write_fd.get());
  return read_fd;
}

// static
void GtkPrimarySelectionOffer::OnOffer(void* data,
                                       gtk_primary_selection_offer* data_offer,
                                       const char* mime_type) {
  auto* self = static_cast<GtkPrimarySelectionOffer*>(data);
  self->AddMimeType(mime_type);
}

}  // namespace ui
