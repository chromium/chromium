// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {

// JSON Keys
constexpr char kEndpointTypeKey[] = "endpoint_type";
constexpr char kUrlKey[] = "url";

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

std::string EndpointTypeToString(EndpointType type) {
  // N.B. If a new EndpointType is added here, please add the relevant entry
  // into |kEndpointStringToTypeMap| within the EndpointStringToType function.
  switch (type) {
    case EndpointType::kDefault:
      return kDefaultString;
    case EndpointType::kUrl:
      return kUrlString;
    case EndpointType::kClipboardHistory:
      return kClipboardHistoryString;
#if BUILDFLAG(IS_CHROMEOS)
    case EndpointType::kUnknownVm:
      return kUnknownVmString;
    case EndpointType::kArc:
      return kArcString;
    case EndpointType::kBorealis:
      return kBorealisString;
    case EndpointType::kCrostini:
      return kCrostiniString;
    case EndpointType::kPluginVm:
      return kPluginVmString;
    case EndpointType::kLacros:
      return kLacrosString;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
}

absl::optional<EndpointType> EndpointStringToType(
    const std::string& endpoint_string) {
  static constexpr auto kEndpointStringToTypeMap =
      base::MakeFixedFlatMap<base::StringPiece, ui::EndpointType>({
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

  auto* it = kEndpointStringToTypeMap.find(endpoint_string);
  if (it != kEndpointStringToTypeMap.end())
    return it->second;

  return {};
}

}  // namespace

std::string ConvertDataTransferEndpointToJson(const DataTransferEndpoint& dte) {
  base::Value encoded_dte(base::Value::Type::DICTIONARY);

  encoded_dte.SetStringKey(kEndpointTypeKey, EndpointTypeToString(dte.type()));

  const GURL* url = dte.GetURL();

  if (url && url->is_valid())
    encoded_dte.SetStringKey(kUrlKey, url->spec());

  std::string json;
  base::JSONWriter::Write(encoded_dte, &json);
  return json;
}

std::unique_ptr<DataTransferEndpoint> ConvertJsonToDataTransferEndpoint(
    std::string json) {
  absl::optional<base::Value> dte_dictionary = base::JSONReader::Read(json);

  if (!dte_dictionary)
    return nullptr;

  const std::string* endpoint_type =
      dte_dictionary->FindStringKey(kEndpointTypeKey);
  const std::string* url_string = dte_dictionary->FindStringKey(kUrlKey);

  if (url_string) {
    GURL url = GURL(*url_string);

    return std::make_unique<DataTransferEndpoint>(url);
  }

  if (endpoint_type && *endpoint_type != kUrlString) {
    if (auto type_enum = EndpointStringToType(*endpoint_type))
      return std::make_unique<DataTransferEndpoint>(type_enum.value());
  }

  return nullptr;
}

}  // namespace ui
