// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_
#define UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_

#include <optional>

#include "build/build_config.h"
#include "url/gurl.h"

namespace ui {

// EndpointType can represent either the source of the transferred data or the
// destination trying to read the data.
// Whenever a new format is supported, a new enum should be added.
enum class EndpointType {
  kDefault = 0,  // This type shouldn't be used if any of the following types is
                 // a better match.
  kUrl = 1,      // Website URL e.g. www.example.com.
  kClipboardHistory = 2,  // Clipboard History UI has privileged access to any
                          // clipboard data.
#if BUILDFLAG(IS_CHROMEOS)
  kUnknownVm = 3,  // The VM type is not identified.
  kArc = 4,        // ARC.
  kBorealis = 5,   // Borealis OS.
  kCrostini = 6,   // Crostini.
  kPluginVm = 7,   // Plugin VM App.
  kLacros = 8,     // Lacros browser.
#endif             // BUILDFLAG(IS_CHROMEOS)
};

struct COMPONENT_EXPORT(UI_BASE_DATA_TRANSFER_POLICY)
    DataTransferEndpointOptions {
  bool notify_if_restricted = true;
  bool off_the_record = false;
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
  // In case DataTransferEndpoint is constructed from a RenderFrameHost object,
  // please use the url of its main frame.
  explicit DataTransferEndpoint(
      const GURL& url,
      DataTransferEndpointOptions options = DataTransferEndpointOptions());
  // This constructor shouldn't be used if |type| == EndpointType::kUrl.
  explicit DataTransferEndpoint(
      EndpointType type,
      DataTransferEndpointOptions options = DataTransferEndpointOptions());

  DataTransferEndpoint(const DataTransferEndpoint& other);
  DataTransferEndpoint(DataTransferEndpoint&& other);

  DataTransferEndpoint& operator=(const DataTransferEndpoint& other);
  DataTransferEndpoint& operator=(DataTransferEndpoint&& other);

  bool operator==(const DataTransferEndpoint& other) const;
  bool operator!=(const DataTransferEndpoint& other) const {
    return !(*this == other);
  }

  ~DataTransferEndpoint();

  bool IsUrlType() const { return type_ == EndpointType::kUrl; }

  const GURL* GetURL() const;

  EndpointType type() const { return type_; }

  bool off_the_record() const { return off_the_record_; }

  bool notify_if_restricted() const { return notify_if_restricted_; }

  // Returns true if both of the endpoints have the same url_ and type_ ==
  // kUrl.
  bool IsSameURLWith(const DataTransferEndpoint& other) const;

 private:
  // This variable should always have a value representing the object type.
  EndpointType type_;

  // The URL of the data endpoint. It always has a value if `type_` ==
  // EndpointType::kUrl, otherwise it's empty.
  std::optional<GURL> url_;

  // Whether the endpoint corresponds to an OTR browser context. This should
  // only be set to true for `EndpointType::kUrl` endpoints.
  bool off_the_record_ = false;

  // This variable should be set to true, if paste is initiated by the user.
  // Otherwise it should be set to false, so the user won't see a notification
  // when the data is restricted by the rules of data leak prevention policy
  // and something in the background is trying to access it.
  bool notify_if_restricted_ = true;
};

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_H_
