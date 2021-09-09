// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "tools/aggregation_service/aggregation_service_tool.h"
#include "url/gurl.h"

namespace {

// If you change any of the switch strings, update the kHelpMsg accordingly.
const char kSwitchContents[] = "contents";
const char kSwitchHelperKeys[] = "helper-keys";
const char kSwitchOutput[] = "output";
const char kSwitchReportingUrl[] = "reporting-url";

const char kHelpMsg[] = R"(
  aggregation_service_tool --contents=<report_contents>
  --helper-keys=<helper_server_keys> [--output=<output_file_path>]
  [--reporting-url=<reporting_url>]

  Example:
  aggregation_service_tool --contents="count-value,1234,5"
  --helper-keys="a.com:keys1.json,b.com:keys2.json" --output="output.json"
  or
  aggregation_service_tool --contents="count-value,1234,5"
  --helper-keys="a.com:keys1.json,b.com:keys2.json"
  --reporting-url="https://c.com"

  aggregation_service_tool is a command-line tool that accepts report contents
  `contents` and mapping of origins to public key json files
  `helper_server_keys` as input and either output an aggregatable report to
  `output_file_path` or send the aggregatable report to `reporting_url`.
)";

void PrintHelp() {
  LOG(INFO) << kHelpMsg;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "aggregation_service_tool");

  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();

  if (args.size() != 0U) {
    LOG(ERROR)
        << "aggregation_service_tool does not expect any additional arguments.";
    PrintHelp();
    return 1;
  }

  if (!command_line.HasSwitch(kSwitchContents)) {
    LOG(ERROR) << "aggregation_service_tool expects " << kSwitchContents
               << " to be specified.";
    PrintHelp();
    return 1;
  }

  if (!command_line.HasSwitch(kSwitchHelperKeys)) {
    LOG(ERROR) << "aggregation_service_tool expects " << kSwitchHelperKeys
               << " to be specified.";
    PrintHelp();
    return 1;
  }

  // Either output or reporting url should be specified, but not both.
  if (!(command_line.HasSwitch(kSwitchOutput) ^
        command_line.HasSwitch(kSwitchReportingUrl))) {
    LOG(ERROR) << "aggregation_service_tool expects either " << kSwitchOutput
               << " or " << kSwitchReportingUrl
               << " to be specified, but not both.";
    PrintHelp();
    return 1;
  }

  aggregation_service::AggregationServiceTool tool;

  std::string helper_keys = command_line.GetSwitchValueASCII(kSwitchHelperKeys);

  // `helper_keys` is formatted like "a.com:keys1.json,b.com:keys2.json".
  base::StringPairs kv_pairs;
  base::SplitStringIntoKeyValuePairs(helper_keys, /*key_value_delimiter=*/':',
                                     /*key_value_pair_delimiter=*/',',
                                     &kv_pairs);
  if (!tool.SetPublicKeys(kv_pairs)) {
    LOG(ERROR) << "aggregation_service_tool failed to set public keys.";
    return 1;
  }

  // TODO(crbug.com/1217824): Interact with the assembler to create an encrypted
  // report.

  base::Value report_contents;

  bool succeeded = false;
  if (command_line.HasSwitch(kSwitchOutput)) {
    base::FilePath output = command_line.GetSwitchValuePath(kSwitchOutput);
    succeeded = tool.WriteReportToFile(report_contents, output);

    if (!succeeded) {
      LOG(ERROR) << "aggregation_service_tool failed to write to " << output
                 << ".";
    }
  } else {
    std::string reporting_url =
        command_line.GetSwitchValueASCII(kSwitchReportingUrl);
    succeeded = tool.SendReport(report_contents, GURL(reporting_url));

    if (!succeeded) {
      LOG(ERROR) << "aggregation_service_tool failed to send the report to "
                 << reporting_url << ".";
    }
  }

  if (!succeeded)
    return 1;

  return 0;
}