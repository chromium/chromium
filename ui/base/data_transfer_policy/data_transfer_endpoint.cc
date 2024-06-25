// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#include <optional>

#include "base/check_op.h"
#include "base/types/optional_util.h"
#include "url/gurl.h"

namespace ui {

DataTransferEndpoint::DataTransferEndpoint(const GURL& url,
                                           DataTransferEndpointOptions options)
    : type_(EndpointType::kUrl),
      url_(url),
      off_the_record_(options.off_the_record),
      notify_if_restricted_(options.notify_if_restricted) {
  DCHECK(url.is_valid());
}

DataTransferEndpoint::DataTransferEndpoint(EndpointType type,
                                           DataTransferEndpointOptions options)
    : type_(type),
      url_(std::nullopt),
      notify_if_restricted_(options.notify_if_restricted) {
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

bool DataTransferEndpoint::operator==(const DataTransferEndpoint& other) const =
    default;

DataTransferEndpoint::~DataTransferEndpoint() = default;

const GURL* DataTransferEndpoint::GetURL() const {
  return base::OptionalToPtr(url_);
}

bool DataTransferEndpoint::IsSameURLWith(
    const DataTransferEndpoint& other) const {
  return IsUrlType() && (type_ == other.type_) && (url_ == other.url_);
}

}  // namespace ui
