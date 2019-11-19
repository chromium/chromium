// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/internal/wayland_data_offer_base.h"

#include "base/stl_util.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {
namespace internal {

namespace {

const char kString[] = "STRING";
const char kText[] = "TEXT";
const char kUtf8String[] = "UTF8_STRING";

}  // namespace

WaylandDataOfferBase::WaylandDataOfferBase() = default;
WaylandDataOfferBase::~WaylandDataOfferBase() = default;

void WaylandDataOfferBase::EnsureTextMimeTypeIfNeeded() {
  if (base::Contains(mime_types_, kMimeTypeText))
    return;

  if (std::any_of(mime_types_.begin(), mime_types_.end(),
                  [](const std::string& mime_type) {
                    return mime_type == kString || mime_type == kText ||
                           mime_type == kMimeTypeTextUtf8 ||
                           mime_type == kUtf8String;
                  })) {
    mime_types_.push_back(kMimeTypeText);
    text_plain_mime_type_inserted_ = true;
  }
}

void WaylandDataOfferBase::AddMimeType(const char* mime_type) {
  mime_types_.push_back(mime_type);
}

}  // namespace internal
}  // namespace ui
