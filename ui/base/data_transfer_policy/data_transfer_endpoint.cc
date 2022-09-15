// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#include "base/check_op.h"
#include "base/types/optional_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ui {

DataTransferEndpoint::DataTransferEndpoint(const GURL& url,
                                           bool notify_if_restricted)
    : type_(EndpointType::kUrl),
      url_(url),
      notify_if_restricted_(notify_if_restricted) {}

DataTransferEndpoint::DataTransferEndpoint(EndpointType type,
                                           bool notify_if_restricted)
    : type_(type),
      url_(absl::nullopt),
      notify_if_restricted_(notify_if_restricted) {
  DCHECK_NE(type, EndpointType::kUrl);
}

DataTransferEndpoint::DataTransferEndpoint(const DataTransferEndpoint& other) =
    default;

DataTransferEndpoint::DataTransferEndpoint(DataTransferEndpoint&& other) =
    default;

DataTransferEndpoint& DataTransferEndpoint::operator=(
    const DataTransferEndpoint& other) = default;

DataTransferEndpoint& DataTransferEndpoint::operator=(
    DataTransferEndpoint&& other) = default;

bool DataTransferEndpoint::operator==(const DataTransferEndpoint& other) const {
  return url_ == other.url_ && type_ == other.type_ &&
         notify_if_restricted_ == other.notify_if_restricted_;
}

DataTransferEndpoint::~DataTransferEndpoint() = default;

const GURL* DataTransferEndpoint::GetURL() const {
  return base::OptionalToPtr(url_);
}

bool DataTransferEndpoint::IsSameURLWith(
    const DataTransferEndpoint& other) const {
  return IsUrlType() && (type_ == other.type_) && (url_ == other.url_);
}

}  // namespace ui
