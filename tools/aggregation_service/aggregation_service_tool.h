// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_
#define TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_

#include <memory>
#include <string>

#include "base/strings/string_split.h"

class GURL;

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace content {
class TestAggregationService;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace aggregation_service {

// This class is a wrapper for aggregation service tool.
class AggregationServiceTool {
 public:
  AggregationServiceTool();
  ~AggregationServiceTool();

  // Sets public keys to storage from the origin-filename pairs and returns
  // whether it's successful.
  bool SetPublicKeys(const base::StringPairs& kv_pairs);

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

  std::unique_ptr<content::TestAggregationService> agg_service_;
};

}  // namespace aggregation_service

#endif  // TOOLS_AGGREGATION_SERVICE_AGGREGATION_SERVICE_TOOL_H_