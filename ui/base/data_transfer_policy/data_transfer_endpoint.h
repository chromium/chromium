// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_
#define UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_

#include "base/optional.h"
#include "base/stl_util.h"
#include "url/origin.h"

namespace ui {

// EndpointType can represent either the source of the transferred data or the
// destination trying to read the data.
// Whenever a new format is supported, a new enum should be added.
enum class EndpointType {
  kDefault = 0,  // This type shouldn't be used if any of the following types is
                 // a better match.
#if defined(OS_CHROMEOS) || (OS_LINUX) || (OS_FUCHSIA)
  kGuestOs = 1,  // Guest OS: PluginVM, Crostini.
#endif           // defined(OS_CHROMEOS) || (OS_LINUX) || (OS_FUCHSIA)
#if defined(OS_CHROMEOS)
  kArc = 2,               // ARC.
#endif                    // defined(OS_CHROMEOS)
  kUrl = 3,               // Website URL e.g. www.example.com.
  kClipboardHistory = 4,  // Clipboard History UI has privileged access to any
                          // clipboard data.
  kMaxValue = kClipboardHistory
};

// DataTransferEndpoint represents:
// - The source of the data being ransferred.
// - The destination trying to access the data.
// - Whether the user should see a notification if the data access is not
// allowed.
// Passing DataTransferEndpoint as a nullptr is equivalent to
// DataTransferEndpoint(kDefault, true). Both specify the same types of
// endpoints (not a URL/ARC++/...etc, and should show a notification to the user
// if the data read is not allowed.)
class COMPONENT_EXPORT(UI_BASE_DATA_TRANSFER_POLICY) DataTransferEndpoint {
 public:
  explicit DataTransferEndpoint(const url::Origin& origin,
                                bool notify_if_restricted = true);
  // This constructor shouldn't be used if |type| == EndpointType::kUrl.
  explicit DataTransferEndpoint(EndpointType type,
                                bool notify_if_restricted = true);

  DataTransferEndpoint(const DataTransferEndpoint& other);
  DataTransferEndpoint(DataTransferEndpoint&& other);

  DataTransferEndpoint& operator=(const DataTransferEndpoint& other) = delete;
  DataTransferEndpoint& operator=(DataTransferEndpoint&& other) = delete;

  bool operator==(const DataTransferEndpoint& other) const;

  ~DataTransferEndpoint();

  bool IsUrlType() const { return type_ == EndpointType::kUrl; }

  const url::Origin* origin() const { return base::OptionalOrNullptr(origin_); }

  EndpointType type() const { return type_; }

  bool notify_if_restricted() const { return notify_if_restricted_; }

 private:
  // This variable should always have a value representing the object type.
  const EndpointType type_;
  // The url::Origin of the data endpoint. It always has a value if `type_` ==
  // EndpointType::kUrl, otherwise it's empty.
  const base::Optional<url::Origin> origin_;
  // This variable should be set to true, if paste is initiated by the user.
  // Otherwise it should be set to false, so the user won't see a notification
  // when the data is restricted by the rules of data leak prevention policy
  // and something in the background is trying to access it.
  bool notify_if_restricted_ = true;
};

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_
