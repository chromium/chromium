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
#include "tools/aggregation_service/aggregation_service_tool_network_initializer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

AggregationServiceTool::AggregationServiceTool()
    : agg_service_(content::TestAggregationService::Create(
          base::DefaultClock::GetInstance())) {}

AggregationServiceTool::~AggregationServiceTool() = default;

bool AggregationServiceTool::SetPublicKeys(const base::StringPairs& kv_pairs) {
  // Send each origin's specified public keys to the tool's storage.
  for (const auto& kv : kv_pairs) {
    url::Origin origin = url::Origin::Create(GURL("https://" + kv.first));
    if (!SetPublicKeysFromFile(origin, kv.second))
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

bool AggregationServiceTool::SendReport(const base::Value& contents,
                                        const GURL& url) {
  DCHECK(url.is_valid());

  ToolNetworkInitializer network_initializer(agg_service_.get());

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
  DCHECK(!filename.empty());

  std::string contents_json;
  JSONStringValueSerializer serializer(&contents_json);
  DCHECK(serializer.Serialize(contents));

  return base::WriteFile(filename, contents_json);
}

}  // namespace aggregation_service