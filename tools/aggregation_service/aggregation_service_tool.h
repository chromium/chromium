// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_
#define TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/values.h"
#include "tools/aggregation_service/aggregation_service_tool_network_initializer.h"
#include "url/origin.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class TestAggregationService;
}  // namespace content

namespace aggregation_service {

struct OriginKeyFile {
  OriginKeyFile(url::Origin origin, std::string key_file);
  OriginKeyFile(const OriginKeyFile& other);
  OriginKeyFile& operator=(const OriginKeyFile& other);
  ~OriginKeyFile();

  url::Origin origin;
  std::string key_file;
};

// This class is a wrapper for aggregation service tool.
class AggregationServiceTool {
 public:
  AggregationServiceTool();
  ~AggregationServiceTool();

  // Sets whether to disable the AggregatableReport's payload(s) being encrypted
  // after serialization.
  void SetDisablePayloadEncryption(bool should_disable);

  // Sets public keys to storage from the origin-filename pairs and returns
  // whether it's successful.
  bool SetPublicKeys(const std::vector<OriginKeyFile>& key_files);

  // Construct an aggregatable report from the specified information and returns
  // a base::Value::DictStorage for its JSON representation. Empty
  // base::Value::DictStorage will be returned in case of error.
  base::Value::DictStorage AssembleReport(
      std::string operation_str,
      std::string bucket_str,
      std::string value_str,
      std::string processing_type_str,
      url::Origin reporting_origin,
      std::string privacy_budget_key,
      std::vector<url::Origin> processing_origins);

  // Sends the contents of the aggregatable report to the specified reporting
  // url `url` and returns whether it's successful.
  bool SendReport(const base::Value& contents, const GURL& url);

  // Writes the contents of the aggregatable report to the specified file
  // `filename` and returns whether it's successful.
  bool WriteReportToFile(const base::Value& contents,
                         const base::FilePath& filename);

 private:
  bool SetPublicKeysFromFile(const url::Origin& origin,
                             const std::string& json_file_path);

  ToolNetworkInitializer network_initializer_;
  std::unique_ptr<content::TestAggregationService> agg_service_;
};

}  // namespace aggregation_service

#endif  // TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_