// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_data_endpoint.h"

#include "base/check_op.h"
#include "base/optional.h"
#include "url/origin.h"

namespace ui {

ClipboardDataEndpoint::ClipboardDataEndpoint(const url::Origin& origin,
                                             bool notify_if_restricted)
    : type_(EndpointType::kUrl),
      origin_(origin),
      notify_if_restricted_(notify_if_restricted) {}

ClipboardDataEndpoint::ClipboardDataEndpoint(EndpointType type,
                                             bool notify_if_restricted)
    : type_(type),
      origin_(base::nullopt),
      notify_if_restricted_(notify_if_restricted) {
  DCHECK_NE(type, EndpointType::kUrl);
}

ClipboardDataEndpoint::ClipboardDataEndpoint(
    const ClipboardDataEndpoint& other) = default;

ClipboardDataEndpoint::ClipboardDataEndpoint(ClipboardDataEndpoint&& other) =
    default;

bool ClipboardDataEndpoint::operator==(
    const ClipboardDataEndpoint& other) const {
  return origin_ == other.origin_ && type_ == other.type_ &&
         notify_if_restricted_ == other.notify_if_restricted_;
}

ClipboardDataEndpoint::~ClipboardDataEndpoint() = default;

}  // namespace ui
