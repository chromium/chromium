// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "tools/aggregation_service/aggregation_service_tool.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// If you change any of the switch strings, update the `kHelpMsg`,
// `kAllowedSwitches` and `kRequiredSwitches` accordingly.
constexpr char kSwitchHelp[] = "help";
constexpr char kSwitchHelpShort[] = "h";
constexpr char kSwitchOperation[] = "operation";
constexpr char kSwitchBucket[] = "bucket";
constexpr char kSwitchValue[] = "value";
constexpr char kSwitchProcessingType[] = "processing-type";
constexpr char kSwitchReportingOrigin[] = "reporting-origin";
constexpr char kSwitchPrivacyBudgetKey[] = "privacy-budget-key";
constexpr char kSwitchHelperKeys[] = "helper-keys";
constexpr char kSwitchOutputFile[] = "output-file";
constexpr char kSwitchOutputUrl[] = "output-url";
constexpr char kDisablePayloadEncryption[] = "disable-payload-encryption";
constexpr char kSwitchAdditionalFields[] = "additional-fields";

constexpr char kHelpMsg[] = R"(
  aggregation_service_tool [--operation=<operation>] --bucket=<bucket>
  --value=<value> --processing-type=<processing_type>
  --reporting-origin=<reporting_origin>
  --privacy-budget-key=<privacy_budget_key>
  --helper-keys=<helper_server_keys> [--output=<output_file>]
  [--output-url=<output_url>] [--disable-payload-encryption]
  [--additional-fields=<additional_fields>]

  Examples:
  aggregation_service_tool --operation="histogram" --bucket=1234 --value=5
  --processing-type="two-party" --reporting-origin="https://example.com"
  --privacy-budget-key="test_privacy_budget_key"
  --helper-keys="https://a.com=keys1.json,https://b.com=keys2.json"
  --output-file="output.json"
  --additional-fields=
  "source_site=https://publisher.example,attribution_destination=https://advertiser.example"
  or
  aggregation_service_tool --bucket=1234 --value=5
  --processing-type="single-server" --reporting-origin="https://example.com"
  --privacy-budget-key="test_privacy_budget_key"
  --helper-keys="https://a.com=keys1.json,https://b.com=keys2.json"
  --output-url="https://c.com/reports"

  aggregation_service_tool is a command-line tool that accepts report contents
  and mapping of origins to public key json files as input and either output an
  aggregatable report to a file on disk or send the aggregatable report to an
  endpoint origin over network. If `--disable-payload-encryption` is specified,
  the aggregatable report's payload(s) will not be encrypted after
  serialization. `scheduled_report_time` will be default to 30 seconds later.

  Switches:
  --operation = Optional switch. Currently only supports "histogram". Default is
                "histogram".
  --bucket = Bucket key of the histogram contribution, must be non-negative
             integer.
  --value = Bucket value of the histogram contribution, must be non-negative
            integer.
  --processing-type = The processing type to use, either "single-server" or
                      "two-party".
  --reporting-origin = The reporting origin endpoint.
  --privacy-budget-key = The privacy budgeting key.
  --helper-keys = List of mapping of origins to public key json files.
  --output-file = Optional switch to specify the output file path.
  --output-url = Optional switch to specify the output url.
  --additional-fields = List of key-value pairs of additional fields to be
                        included in the aggregatable report. Only supports
                        string valued fields.
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

  const std::vector<std::string> kAllowedSwitches = {kSwitchHelp,
                                                     kSwitchHelpShort,
                                                     kSwitchOperation,
                                                     kSwitchBucket,
                                                     kSwitchValue,
                                                     kSwitchProcessingType,
                                                     kSwitchReportingOrigin,
                                                     kSwitchPrivacyBudgetKey,
                                                     kSwitchHelperKeys,
                                                     kSwitchOutputFile,
                                                     kSwitchOutputUrl,
                                                     kDisablePayloadEncryption,
                                                     kSwitchAdditionalFields};
  for (const auto& provided_switch : command_line.GetSwitches()) {
    if (!base::Contains(kAllowedSwitches, provided_switch.first)) {
      LOG(ERROR) << "aggregation_service_tool did not expect "
                 << provided_switch.first << " to be specified.";
      PrintHelp();
      return 1;
    }
  }

  if (command_line.GetSwitches().empty() ||
      command_line.HasSwitch(kSwitchHelp) ||
      command_line.HasSwitch(kSwitchHelpShort)) {
    PrintHelp();
    return 1;
  }

  const std::vector<std::string> kRequiredSwitches = {
      kSwitchBucket,           kSwitchValue,
      kSwitchProcessingType,   kSwitchReportingOrigin,
      kSwitchPrivacyBudgetKey, kSwitchHelperKeys};
  for (const std::string& required_switch : kRequiredSwitches) {
    if (!command_line.HasSwitch(required_switch.c_str())) {
      LOG(ERROR) << "aggregation_service_tool expects " << required_switch
                 << " to be specified.";
      PrintHelp();
      return 1;
    }
  }

  // Either output or reporting url should be specified, but not both.
  if (!(command_line.HasSwitch(kSwitchOutputFile) ^
        command_line.HasSwitch(kSwitchOutputUrl))) {
    LOG(ERROR) << "aggregation_service_tool expects either "
               << kSwitchOutputFile << " or " << kSwitchOutputUrl
               << " to be specified, but not both.";
    PrintHelp();
    return 1;
  }

  aggregation_service::AggregationServiceTool tool;

  tool.SetDisablePayloadEncryption(
      /*should_disable=*/command_line.HasSwitch(kDisablePayloadEncryption));

  std::string helper_keys = command_line.GetSwitchValueASCII(kSwitchHelperKeys);

  // `helper_keys` is formatted like
  // "https://a.com=keys1.json,https://b.com=keys2.json".
  base::StringPairs kv_pairs;
  base::SplitStringIntoKeyValuePairs(helper_keys, /*key_value_delimiter=*/'=',
                                     /*key_value_pair_delimiter=*/',',
                                     &kv_pairs);

  std::vector<aggregation_service::OriginKeyFile> key_files;
  std::vector<url::Origin> processing_origins;
  for (auto& kv : kv_pairs) {
    url::Origin origin = url::Origin::Create(GURL(kv.first));
    key_files.emplace_back(origin, std::move(kv.second));
    processing_origins.push_back(origin);
  }

  if (!tool.SetPublicKeys(key_files)) {
    LOG(ERROR) << "aggregation_service_tool failed to set public keys.";
    return 1;
  }

  std::string operation =
      command_line.HasSwitch(kSwitchOperation)
          ? command_line.GetSwitchValueASCII(kSwitchOperation)
          : "histogram";

  std::string processing_type =
      command_line.HasSwitch(kSwitchProcessingType)
          ? command_line.GetSwitchValueASCII(kSwitchProcessingType)
          : "two-party";

  url::Origin reporting_origin = url::Origin::Create(
      GURL(command_line.GetSwitchValueASCII(kSwitchReportingOrigin)));

  std::string privacy_budget_key =
      command_line.GetSwitchValueASCII(kSwitchPrivacyBudgetKey);

  base::Value::DictStorage report_dict = tool.AssembleReport(
      std::move(operation), command_line.GetSwitchValueASCII(kSwitchBucket),
      command_line.GetSwitchValueASCII(kSwitchValue),
      std::move(processing_type), std::move(reporting_origin),
      std::move(privacy_budget_key), std::move(processing_origins));
  if (report_dict.empty()) {
    LOG(ERROR)
        << "aggregation_service_tool failed to create the aggregatable report.";
    return 1;
  }

  if (command_line.HasSwitch(kSwitchAdditionalFields)) {
    std::string additional_fields =
        command_line.GetSwitchValueASCII(kSwitchAdditionalFields);
    // `additional_fields` is formatted like "key1=value1,key2=value2".
    base::StringPairs kv_pairs;
    base::SplitStringIntoKeyValuePairs(
        additional_fields, /*key_value_delimiter=*/'=',
        /*key_value_pair_delimiter=*/',', &kv_pairs);
    for (std::pair<std::string, std::string>& kv : kv_pairs) {
      report_dict.emplace(std::move(kv.first), std::move(kv.second));
    }
  }

  base::Value report_contents(std::move(report_dict));

  bool succeeded = false;
  if (command_line.HasSwitch(kSwitchOutputFile)) {
    base::FilePath output_file =
        command_line.GetSwitchValuePath(kSwitchOutputFile);
    succeeded = tool.WriteReportToFile(report_contents, output_file);

    if (!succeeded) {
      LOG(ERROR) << "aggregation_service_tool failed to write to "
                 << output_file << ".";
    }
  } else {
    std::string output_url = command_line.GetSwitchValueASCII(kSwitchOutputUrl);
    succeeded = tool.SendReport(report_contents, GURL(output_url));

    if (!succeeded) {
      LOG(ERROR) << "aggregation_service_tool failed to send the report to "
                 << output_url << ".";
    }
  }

  if (!succeeded)
    return 1;

  return 0;
}