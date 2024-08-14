// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/aggregation_service/aggregation_service_tool.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/test/test_aggregation_service.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

namespace {

std::optional<content::TestAggregationService::Operation> ConvertToOperation(
    std::string_view operation_string) {
  if (operation_string == "histogram")
    return content::TestAggregationService::Operation::kHistogram;

  return std::nullopt;
}

std::optional<content::TestAggregationService::AggregationMode>
ConvertToAggregationMode(std::string_view aggregation_mode_string) {
  if (aggregation_mode_string == "tee-based")
    return content::TestAggregationService::AggregationMode::kTeeBased;
  if (aggregation_mode_string == "experimental-poplar")
    return content::TestAggregationService::AggregationMode::
        kExperimentalPoplar;
  if (aggregation_mode_string == "default")
    return content::TestAggregationService::AggregationMode::kDefault;

  return std::nullopt;
}

}  // namespace

UrlKeyFile::UrlKeyFile(GURL url, std::string key_file)
    : url(std::move(url)), key_file(std::move(key_file)) {}

UrlKeyFile::UrlKeyFile(const UrlKeyFile& other) = default;

UrlKeyFile& UrlKeyFile::operator=(const UrlKeyFile& other) = default;

UrlKeyFile::~UrlKeyFile() = default;

AggregationServiceTool::AggregationServiceTool()
    : agg_service_(content::TestAggregationService::Create(
          base::DefaultClock::GetInstance(),
          network_initializer_.shared_url_loader_factory())) {}

AggregationServiceTool::~AggregationServiceTool() = default;

void AggregationServiceTool::SetDisablePayloadEncryption(bool should_disable) {
  agg_service_->SetDisablePayloadEncryption(should_disable);
}

bool AggregationServiceTool::SetPublicKeys(
    const std::vector<UrlKeyFile>& key_files) {
  // Send each url's specified public keys to the tool's storage.
  for (const auto& key_file : key_files) {
    if (!network::IsUrlPotentiallyTrustworthy(key_file.url)) {
      LOG(ERROR) << "Invalid processing url: " << key_file.url;
      return false;
    }

    if (!SetPublicKeysFromFile(key_file.url, key_file.key_file))
      return false;
  }

  return true;
}

bool AggregationServiceTool::SetPublicKeysFromFile(
    const GURL& url,
    std::string_view json_file_path) {
#if BUILDFLAG(IS_WIN)
  base::FilePath json_file(base::UTF8ToWide(json_file_path));
#else
  base::FilePath json_file(json_file_path);
#endif

  bool succeeded = false;

  base::RunLoop run_loop;
  agg_service_->SetPublicKeys(
      url, json_file,
      base::BindOnce(
          [](base::OnceClosure quit, bool& succeeded_out, bool succeeded_in) {
            succeeded_out = succeeded_in;
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), std::ref(succeeded)));
  run_loop.Run();

  return succeeded;
}

base::Value::Dict AggregationServiceTool::AssembleReport(
    std::string operation_str,
    std::string bucket_str,
    std::string value_str,
    std::string aggregation_mode_str,
    url::Origin reporting_origin,
    std::vector<GURL> processing_urls,
    bool is_debug_mode_enabled,
    base::Value::Dict additional_fields,
    std::string api_version,
    std::string api_identifier) {
  base::Value::Dict result;

  std::optional<content::TestAggregationService::Operation> operation =
      ConvertToOperation(operation_str);
  if (!operation.has_value()) {
    LOG(ERROR) << "Invalid operation: " << operation_str;
    return result;
  }

  absl::uint128 bucket;
  if (!base::StringToUint128(bucket_str, &bucket)) {
    LOG(ERROR) << "Invalid bucket: " << bucket_str;
    return result;
  }

  int value;
  if (!base::StringToInt(value_str, &value) || value < 0) {
    LOG(ERROR) << "Invalid value: " << value_str;
    return result;
  }

  std::optional<content::TestAggregationService::AggregationMode>
      aggregation_mode = ConvertToAggregationMode(aggregation_mode_str);
  if (!aggregation_mode.has_value()) {
    LOG(ERROR) << "Invalid aggregation mode: " << aggregation_mode_str;
    return result;
  }

  if (reporting_origin.opaque()) {
    LOG(ERROR) << "Invalid reporting origin: " << reporting_origin;
    return result;
  }

  content::TestAggregationService::AssembleRequest request(
      operation.value(), bucket, value, aggregation_mode.value(),
      std::move(reporting_origin), std::move(processing_urls),
      is_debug_mode_enabled, std::move(additional_fields),
      std::move(api_version), std::move(api_identifier));

  base::RunLoop run_loop;
  agg_service_->AssembleReport(
      std::move(request),
      base::BindOnce(
          [](base::OnceClosure quit, base::Value::Dict& result_out,
             base::Value::Dict result_in) {
            result_out = std::move(result_in);
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), std::ref(result)));
  run_loop.Run();

  return result;
}

bool AggregationServiceTool::SendReport(const base::Value& contents,
                                        const GURL& url) {
  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid output url: " << url;
    return false;
  }

  bool succeeded = false;

  base::RunLoop run_loop;
  agg_service_->SendReport(
      url, contents,
      base::BindOnce(
          [](base::OnceClosure quit, bool& succeeded_out, bool succeeded_in) {
            succeeded_out = succeeded_in;
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), std::ref(succeeded)));
  run_loop.Run();

  return succeeded;
}

bool AggregationServiceTool::WriteReportToFile(const base::Value& contents,
                                               const base::FilePath& filename) {
  if (filename.empty()) {
    LOG(ERROR) << "Invalid output file: " << filename;
    return false;
  }

  std::string contents_json;
  JSONStringValueSerializer serializer(&contents_json);
  CHECK(serializer.Serialize(contents));

  return base::WriteFile(filename, contents_json);
}

}  // namespace aggregation_service
