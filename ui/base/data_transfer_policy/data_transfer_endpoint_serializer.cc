// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {

// JSON Keys
constexpr char kEndpointTypeKey[] = "endpoint_type";
constexpr char kUrlKey[] = "url";
constexpr char kOffTheRecord[] = "off_the_record";

// Endpoint Type Strings
constexpr char kDefaultString[] = "default";
constexpr char kUrlString[] = "url";
constexpr char kClipboardHistoryString[] = "clipboard_history";
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kUnknownVmString[] = "unknown_vm";
constexpr char kArcString[] = "arc";
constexpr char kBorealisString[] = "borealis";
constexpr char kCrostiniString[] = "crostini";
constexpr char kPluginVmString[] = "plugin_vm";
constexpr char kLacrosString[] = "lacros";
#endif  // BUILDFLAG(IS_CHROMEOS)

std::optional<EndpointType> EndpointStringToType(
    const std::string& endpoint_string) {
  static constexpr auto kEndpointStringToTypeMap =
      base::MakeFixedFlatMap<std::string_view, ui::EndpointType>({
#if BUILDFLAG(IS_CHROMEOS)
          {kUnknownVmString, EndpointType::kUnknownVm},
          {kArcString, EndpointType::kArc},
          {kBorealisString, EndpointType::kBorealis},
          {kCrostiniString, EndpointType::kCrostini},
          {kPluginVmString, EndpointType::kPluginVm},
          {kLacrosString, EndpointType::kLacros},
#endif  // BUILDFLAG(IS_CHROMEOS)
          {kDefaultString, EndpointType::kDefault},
          {kUrlString, EndpointType::kUrl},
          {kClipboardHistoryString, EndpointType::kClipboardHistory},
      });

  auto it = kEndpointStringToTypeMap.find(endpoint_string);
  if (it != kEndpointStringToTypeMap.end())
    return it->second;

  return {};
}

}  // namespace

std::unique_ptr<DataTransferEndpoint> ConvertJsonToDataTransferEndpoint(
    std::string json) {
  std::optional<base::Value> dte_dictionary = base::JSONReader::Read(json);

  if (!dte_dictionary) {
    return nullptr;
  }

  const std::string* endpoint_type =
      dte_dictionary->GetDict().FindString(kEndpointTypeKey);
  if (!endpoint_type) {
    return nullptr;
  }

  if (*endpoint_type == kUrlString) {
    const std::string* url_string =
        dte_dictionary->GetDict().FindString(kUrlKey);
    if (!url_string) {
      return nullptr;
    }

    GURL url = GURL(*url_string);
    if (!url.is_valid()) {
      return nullptr;
    }

    return std::make_unique<DataTransferEndpoint>(
        url, DataTransferEndpointOptions{.off_the_record =
                                             dte_dictionary->GetDict()
                                                 .FindBool(kOffTheRecord)
                                                 .value_or(false)});
  }

  auto type_enum = EndpointStringToType(*endpoint_type);
  if (!type_enum) {
    return nullptr;
  }

  return std::make_unique<DataTransferEndpoint>(type_enum.value());
}

}  // namespace ui
