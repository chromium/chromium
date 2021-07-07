// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "content/public/test/test_aggregation_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// If you change any of the switch strings, update the kHelpMsg accordingly.
const char kSwitchContents[] = "contents";
const char kSwitchHelperKeys[] = "helper-keys";
const char kSwitchOutput[] = "output";

const char kHelpMsg[] = R"(
  aggregation_service_tool --contents=<report_contents>
  --helper-keys=<helper_server_keys> --output=<output_file_path>

  Example:
  aggregation_service_tool --contents="count-value,1234,5"
  --helper-keys="a.com:keys1.json,b.com:keys2.json" --output="output.json"

  aggregation_service_tool is a command-line tool that accepts report contents
  `contents` and mapping of origins to public key json files
  `helper_server_keys` as input and output an encrypted report in
  `output_file_path`.
)";

void PrintHelp() {
  LOG(INFO) << kHelpMsg;
}

void SetPublicKeysFromFile(const url::Origin& origin,
                           const std::string& json_file_path,
                           content::TestAggregationService* agg_service,
                           base::OnceCallback<void(bool)> callback) {
#if defined(OS_WIN)
  base::FilePath json_file(base::UTF8ToWide(json_file_path));
#else
  base::FilePath json_file(json_file_path);
#endif

  if (!base::PathExists(json_file)) {
    LOG(ERROR) << "aggregation_service_tool failed to open file: "
               << json_file.value() << ".";
    std::move(callback).Run(false);
    return;
  }

  std::string json_string;
  if (!base::ReadFileToString(json_file, &json_string)) {
    LOG(ERROR) << "aggregation_service_tool failed to read file: "
               << json_file.value() << ".";
    std::move(callback).Run(false);
    return;
  }

  agg_service->SetPublicKeys(origin, json_string, std::move(callback));
}

}  // namespace

int main(int argc, char* argv[]) {
  base::SingleThreadTaskExecutor executor;
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

  if (!command_line.HasSwitch(kSwitchContents) ||
      !command_line.HasSwitch(kSwitchHelperKeys) ||
      !command_line.HasSwitch(kSwitchOutput)) {
    LOG(ERROR) << "aggregation_service_tool expects contents, helper keys and "
                  "output to be specified.";
    PrintHelp();
    return 1;
  }

  std::string contents = command_line.GetSwitchValueASCII(kSwitchContents);
  std::string helper_keys = command_line.GetSwitchValueASCII(kSwitchHelperKeys);
  base::FilePath output = command_line.GetSwitchValuePath(kSwitchOutput);

  std::unique_ptr<content::TestAggregationService> agg_service =
      content::TestAggregationService::Create();

  // `helper_keys` is formatted like "a.com:keys1.json,b.com:keys2.json".
  base::StringPairs kv_pairs;
  base::SplitStringIntoKeyValuePairs(helper_keys, /*key_value_delimiter=*/':',
                                     /*key_value_pair_delimiter=*/',',
                                     &kv_pairs);

  // Send each origin's specified public keys to the tool's storage.
  for (const auto& kv : kv_pairs) {
    url::Origin origin = url::Origin::Create(GURL("https://" + kv.first));
    bool succeeded = false;
    base::RunLoop run_loop;
    SetPublicKeysFromFile(
        origin, kv.second, agg_service.get(),
        base::BindOnce(
            [](base::OnceClosure quit, bool& succeeded_out, bool succeeded_in) {
              succeeded_out = succeeded_in;
              std::move(quit).Run();
            },
            run_loop.QuitClosure(), std::ref(succeeded)));
    run_loop.Run();
    if (!succeeded) {
      LOG(ERROR)
          << "aggregation_service_tool failed to set public keys for origin: "
          << origin << ".";
      return 1;
    }
  }

  // TODO(crbug.com/1217824): Interact with the assembler to create an encrypted
  // report.

  // TODO(crbug.com/1218124): Returning that report (e.g. by saving to disk).

  return 0;
}
