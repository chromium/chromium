// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_SERIALIZER_H_
#define UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_SERIALIZER_H_

#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

// The MIME Type chromium/x-data-transfer-endpoint holds metadata
// from DataTransferEndpoint objects. It is currently used as a
// communication medium to transfer information about clipboard sources.
//
// The MIME type data is a JSON string in the form:
// {
//   "endpoint_type": "<endpoint type>",
//   "url_origin": "https://www.google.com",
//   "url": "https://www.google.com"
// }
// TODO(crbug.com/1300476): "url_origin" is still being sent because of the
// version skew between Lacros and ash. It should be removed after M102.

namespace ui {

// Encodes DataTransferEndpoints into a JSON string in the format as described
// above.
COMPONENT_EXPORT(UI_BASE_DATA_TRANSFER_POLICY)
std::string ConvertDataTransferEndpointToJson(const DataTransferEndpoint& dte);

// Decodes JSON strings into DataTransferEndpoint objects.
// If no type or origin found, nullptr is returned.
COMPONENT_EXPORT(UI_BASE_DATA_TRANSFER_POLICY)
std::unique_ptr<DataTransferEndpoint> ConvertJsonToDataTransferEndpoint(
    std::string json);

}  // namespace ui

#endif  // UI_BASE_DATA_TRANSFER_POLICY_DATA_TRANSFER_ENDPOINT_SERIALIZER_H_
