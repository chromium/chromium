// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/aggregation_service/aggregation_service_tool.h"

#include <functional>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/test/test_aggregation_service.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

namespace {

absl::optional<content::TestAggregationService::Operation> ConvertToOperation(
    const std::string& operation_string) {
  if (operation_string == "histogram")
    return content::TestAggregationService::Operation::kHistogram;

  return absl::nullopt;
}

absl::optional<content::TestAggregationService::ProcessingType>
ConvertToProcessingType(const std::string& processing_type_string) {
  if (processing_type_string == "two-party")
    return content::TestAggregationService::ProcessingType::kTwoParty;
  if (processing_type_string == "single-server")
    return content::TestAggregationService::ProcessingType::kSingleServer;

  return absl::nullopt;
}

}  // namespace

OriginKeyFile::OriginKeyFile(url::Origin origin, std::string key_file)
    : origin(std::move(origin)), key_file(std::move(key_file)) {}

OriginKeyFile::OriginKeyFile(const OriginKeyFile& other) = default;

OriginKeyFile& OriginKeyFile::operator=(const OriginKeyFile& other) = default;

OriginKeyFile::~OriginKeyFile() = default;

AggregationServiceTool::AggregationServiceTool()
    : agg_service_(content::TestAggregationService::Create(
          base::DefaultClock::GetInstance(),
          network_initializer_.shared_url_loader_factory())) {}

AggregationServiceTool::~AggregationServiceTool() = default;

void AggregationServiceTool::SetDisablePayloadEncryption(bool should_disable) {
  agg_service_->SetDisablePayloadEncryption(should_disable);
}

bool AggregationServiceTool::SetPublicKeys(
    const std::vector<OriginKeyFile>& key_files) {
  // Send each origin's specified public keys to the tool's storage.
  for (const auto& key_file : key_files) {
    if (!network::IsOriginPotentiallyTrustworthy(key_file.origin)) {
      LOG(ERROR) << "Invalid processing origin: " << key_file.origin;
      return false;
    }

    if (!SetPublicKeysFromFile(key_file.origin, key_file.key_file))
      return false;
  }

  return true;
}

bool AggregationServiceTool::SetPublicKeysFromFile(
    const url::Origin& origin,
    const std::string& json_file_path) {
#if defined(OS_WIN)
  base::FilePath json_file(base::UTF8ToWide(json_file_path));
#else
  base::FilePath json_file(json_file_path);
#endif

  if (!base::PathExists(json_file)) {
    LOG(ERROR) << "aggregation_service_tool failed to open file: "
               << json_file.value() << ".";
    return false;
  }

  std::string json_string;
  if (!base::ReadFileToString(json_file, &json_string)) {
    LOG(ERROR) << "aggregation_service_tool failed to read file: "
               << json_file.value() << ".";
    return false;
  }

  bool succeeded = false;

  base::RunLoop run_loop;
  agg_service_->SetPublicKeys(
      origin, json_string,
      base::BindOnce(
          [](base::OnceClosure quit, bool& succeeded_out, bool succeeded_in) {
            succeeded_out = succeeded_in;
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), std::ref(succeeded)));
  run_loop.Run();

  return succeeded;
}

base::Value::DictStorage AggregationServiceTool::AssembleReport(
    std::string operation_str,
    std::string bucket_str,
    std::string value_str,
    std::string processing_type_str,
    url::Origin reporting_origin,
    std::string privacy_budget_key,
    std::vector<url::Origin> processing_origins) {
  base::Value::DictStorage result;

  absl::optional<content::TestAggregationService::Operation> operation =
      ConvertToOperation(operation_str);
  if (!operation.has_value()) {
    LOG(ERROR) << "Invalid operation: " << operation_str;
    return result;
  }

  int bucket = 0;
  if (!base::StringToInt(bucket_str, &bucket) || bucket < 0) {
    LOG(ERROR) << "Invalid bucket: " << bucket_str;
    return result;
  }

  int value = 0;
  if (!base::StringToInt(value_str, &value) || value < 0) {
    LOG(ERROR) << "Invalid value: " << value_str;
    return result;
  }

  absl::optional<content::TestAggregationService::ProcessingType>
      processing_type = ConvertToProcessingType(processing_type_str);
  if (!processing_type.has_value()) {
    LOG(ERROR) << "Invalid processing type: " << processing_type_str;
    return result;
  }

  if (reporting_origin.opaque()) {
    LOG(ERROR) << "Invalid reporting origin: " << reporting_origin;
    return result;
  }

  content::TestAggregationService::AssembleRequest request(
      operation.value(), bucket, value, processing_type.value(),
      std::move(reporting_origin), std::move(privacy_budget_key),
      std::move(processing_origins));

  base::RunLoop run_loop;
  agg_service_->AssembleReport(
      std::move(request),
      base::BindOnce(
          [](base::OnceClosure quit, base::Value::DictStorage& result_out,
             base::Value::DictStorage result_in) {
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
  DCHECK(serializer.Serialize(contents));

  return base::WriteFile(filename, contents_json);
}

}  // namespace aggregation_service