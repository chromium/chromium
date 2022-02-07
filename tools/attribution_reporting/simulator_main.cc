// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/attribution_reporting.h"
#include "content/public/test/attribution_simulator.h"

namespace {

constexpr char kSwitchHelp[] = "help";
constexpr char kSwitchHelpShort[] = "h";

constexpr char kSwitchVersion[] = "version";
constexpr char kSwitchVersionShort[] = "v";

constexpr char kSwitchDelayMode[] = "delay_mode";
constexpr char kSwitchInputFile[] = "input_file";
constexpr char kSwitchNoiseMode[] = "noise_mode";
constexpr char kSwitchRemoveReportIds[] = "remove_report_ids";

constexpr const char* kAllowedSwitches[] = {
    kSwitchHelp,      kSwitchHelpShort,
    kSwitchVersion,   kSwitchVersionShort,

    kSwitchInputFile, kSwitchDelayMode,
    kSwitchNoiseMode, kSwitchRemoveReportIds,
};

constexpr const char* kRequiredSwitches[] = {
    kSwitchInputFile,
};

constexpr char kHelpMsg[] = R"(
attribution_reporting_simulator --input_file=<input_file>
  [--delay_mode=<mode>]
  [--noise_mode=<mode>]
  [--remove_report_ids]

attribution_reporting_simulator is a command-line tool that simulates the
Attribution Reporting API for a single user on sources and triggers specified
in an input file. It writes the generated reports, if any, to stdout, with
associated metadata.

Sources and triggers are registered in chronological order according to their
`source_time` and `trigger_time` fields, respectively.

Learn more about the Attribution Reporting API at
https://github.com/WICG/conversion-measurement-api#attribution-reporting-api.

Learn about the meaning of the input and output fields at
https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md.

Switches:
  --input_file=<input_file> - Required path to a JSON file containing sources
                              and triggers to register in the simulation.
                              Input format described below.

  --delay_mode=<mode>      -  Optional. One of `default` or `none`. Defaults to
                              `default`.

                              default: Reports are sent in reporting windows
                              some time after attribution is triggered.

                              none: Reports are sent immediately after
                              attribution is triggered.

  --noise_mode=<mode>       - Optional. One of `default` or `none`. Defaults to
                              `default`.

                              default: Sources are subject to randomized
                              response, reports within a reporting window are
                              shuffled.

                              none: None of the above applies.

  --remove_report_ids       - Optional. If present, removes the `report_id`
                              field from report bodies, as they are randomly
                              generated. Use this switch to make the tool's
                              output more deterministic.

  --version                 - Outputs the tool version and exits.

Input format:

{
  // List of zero or more sources to register.
  "sources": [
    {
      // Required time at which to register the source in seconds since the
      // UNIX epoch.
      "source_time": 123,

      // Required origin on which to register the source.
      "source_origin": "https://source.example",

      // Required source type, either "navigation" or "event", corresponding to
      // whether the source is registered on click or on view, respectively.
      "source_type": "navigation",

      "registration_config": {
        // Required uint64 formatted as a base-10 string.
        "source_event_id": "123456789",

        // Required site on which the source will be attributed.
        "destination": "https://destination.example",

        // Required origin to which the report will be sent if the source is
        // attributed.
        "reporting_origin": "https://reporting.example",

        // Optional int64 in milliseconds formatted as a base-10 string.
        // Defaults to 30 days.
        "expiry": "864000000",

        // Optional int64 formatted as a base-10 string.
        // Defaults to 0.
        "priority": "-456"
      }
    },
    ...
  ],

  // List of zero or more triggers to register.
  "triggers": [
    {
      // Required time at which to register the trigger in seconds since the
      // UNIX epoch.
      "trigger_time": 123,

      // Required site on which the trigger is being registered.
      "destination": "https://destination.example",

      // Required origin to which the report will be sent.
      "reporting_origin": "https://reporting.example",

      "registration_config": {
        // Optional uint64 formatted as a base-10 string.
        // Defaults to 0.
        "trigger_data": "3",

        // Optional uint64 formatted as a base-10 string.
        // Defaults to 0.
        "event_source_trigger_data": "1",

        // Optional int64 formatted as a base-10 string.
        // Defaults to 0.
        "priority": "-456",

        // Optional int64 formatted as a base-10 string.
        // Defaults to null.
        "dedup_key": "789"
      }
    },
    ...
  ]
}

Output format:

{
  // List of zero or more reports.
  reports: [
    {
      // Time at which the report would have been sent in seconds since the
      // UNIX epoch.
      "report_time": 123,

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-attribution",

      // The report itself. See
      // https://github.com/WICG/conversion-measurement-api/blob/main/EVENT.md#attribution-reports
      // for details about its fields.
      "report": { ... }
      },
    },
    ...
  ]
}
)";

void PrintHelp() {
  std::cerr << kHelpMsg;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.GetArgs().empty()) {
    std::cerr << "unexpected additional arguments" << std::endl;
    PrintHelp();
    return 1;
  }

  for (const auto& provided_switch : command_line.GetSwitches()) {
    if (!base::Contains(kAllowedSwitches, provided_switch.first)) {
      std::cerr << "unexpected switch `" << provided_switch.first << "`"
                << std::endl;
      PrintHelp();
      return 1;
    }
  }

  if (command_line.GetSwitches().empty() ||
      command_line.HasSwitch(kSwitchHelp) ||
      command_line.HasSwitch(kSwitchHelpShort)) {
    PrintHelp();
    return 0;
  }

  if (command_line.HasSwitch(kSwitchVersion) ||
      command_line.HasSwitch(kSwitchVersionShort)) {
    std::cout << version_info::GetVersionNumber() << std::endl;
    return 0;
  }

  for (const char* required_switch : kRequiredSwitches) {
    if (!command_line.HasSwitch(required_switch)) {
      std::cerr << "missing required switch `" << required_switch << "`"
                << std::endl;
      PrintHelp();
      return 1;
    }
  }

  auto noise_mode = content::AttributionNoiseMode::kDefault;
  if (command_line.HasSwitch(kSwitchNoiseMode)) {
    std::string str = command_line.GetSwitchValueASCII(kSwitchNoiseMode);

    if (str == "none") {
      noise_mode = content::AttributionNoiseMode::kNone;
    } else if (str != "default") {
      std::cerr << "unknown noise mode: " << str << std::endl;
      return 1;
    }
  }

  auto delay_mode = content::AttributionDelayMode::kDefault;
  if (command_line.HasSwitch(kSwitchDelayMode)) {
    std::string str = command_line.GetSwitchValueASCII(kSwitchDelayMode);

    if (str == "none") {
      delay_mode = content::AttributionDelayMode::kNone;
    } else if (str != "default") {
      std::cerr << "unknown report mode: " << str << std::endl;
      return 1;
    }
  }

  std::string error_msg;
  std::unique_ptr<base::Value> input =
      JSONFileValueDeserializer(
          command_line.GetSwitchValuePath(kSwitchInputFile))
          .Deserialize(nullptr, &error_msg);
  if (!input) {
    std::cerr << "failed to read input file: " << error_msg << std::endl;
    return 1;
  }

  base::Value output = content::RunAttributionSimulationOrExit(
      *input,
      content::AttributionSimulationOptions{
          .noise_mode = noise_mode,
          .delay_mode = delay_mode,
          .remove_report_ids = command_line.HasSwitch(kSwitchRemoveReportIds),
      });

  std::string output_json;
  bool success = base::JSONWriter::WriteWithOptions(
      output, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output_json);
  if (!success) {
    std::cerr << "failed to serialize output JSON";
    return 1;
  }

  std::cout << output_json;
  return 0;
}
