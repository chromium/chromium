// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

WaylandDataOfferBase::WaylandDataOfferBase() = default;
WaylandDataOfferBase::~WaylandDataOfferBase() = default;

void WaylandDataOfferBase::EnsureTextMimeTypeIfNeeded() {
  if (base::Contains(mime_types_, kMimeTypePlainText)) {
    return;
  }

  if (std::ranges::any_of(mime_types_, [](const std::string& mime_type) {
        return mime_type == kMimeTypeLinuxString ||
               mime_type == kMimeTypeLinuxText ||
               mime_type == kMimeTypeUtf8PlainText ||
               mime_type == kMimeTypeLinuxUtf8String;
      })) {
    mime_types_.push_back(kMimeTypePlainText);
    text_plain_mime_type_inserted_ = true;
  }
}

void WaylandDataOfferBase::AddMimeType(const char* mime_type) {
  mime_types_.push_back(mime_type);
}

}  // namespace ui
