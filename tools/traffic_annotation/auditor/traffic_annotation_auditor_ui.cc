// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_exporter.h"

namespace {

const char* HELP_TEXT = R"(
Traffic Annotation Auditor
Extracts network traffic annotations from the repository, audits them for errors
and coverage, produces reports, and updates related files.

Usage: traffic_annotation_auditor [OPTION]... [path_filters]

Extracts network traffic annotations from source files. If path filter(s) are
specified, only those directories of the source will be analyzed.

Options:
  -h, --help          Shows help.
  --build-path        Path to the build directory.
  --source-path       Optional path to the src directory. If not provided and
                      build-path is available, assumed to be 'build-path/../..',
                      otherwise current directory.
  --tool-path         Optional path to traffic_annotation_extractor clang tool.
                      If not specified, it's assumed to be in the same path as
                      auditor's executable.
  --extractor-output  Optional path to the temporary file that extracted
                      annotations will be stored into.
  --extractor-input   Optional path to the file that temporary extracted
                      annotations are already stored in. If this is provided,
                      clang tool is not run and this is used as input.
  --no-filtering      Optional flag asking the tool to run on the whole
                      repository without text filtering files. Using this flag
                      may increase processing time x40.
  --all-files         Optional flag asking to use compile_commands.json instead
                      of git to get the list of files that will be checked.
                      This flag is useful when using build flags that change
                      files, like jumbo. This flag slows down the execution as
                      it checks every compiled file.
  --test-only         Optional flag to request just running tests and not
                      updating any file. If not specified,
                      'tools/traffic_annotation/summary/annotations.xml' might
                      get updated.
  --errors-file       Optional file path for possible errors. If not specified,
                      errors are dumped to LOG(ERROR).
  --no-missing-error  Optional argument, resulting in just issuing a warning for
                      functions that miss annotation and not an error.
  --summary-file      Optional path to the output file with all annotations.
  --annotations-file  Optional path to a TSV output file with all annotations.
  --limit             Limit for the maximum number of returned errors.
                      Use 0 for unlimited.
  --error-resilient   Optional flag, stating not to return error in exit code if
                      auditor fails to perform the tests. This flag can be used
                      for trybots to avoid spamming when tests cannot run.
  --extractor-backend=[clang_tool,python_script]
                      Optional flag specifying which backend to use for
                      extracting annotation definitions from source code (Clang
                      Tool or extractor.py). Defaults to "python_script".
  path_filters        Optional paths to filter which files the tool is run on.
                      It can also include deleted files names when auditor is
                      run on a partial repository. These are ignored if all of
                      the following are true:
                        - Not using --extractor-input
                        - Using --no-filtering OR --all-files
                        - Using the python extractor

Example:
  traffic_annotation_auditor --build-path=out/Release
)";

const std::string kCodeSearchLink("https://cs.chromium.org/chromium/src/");

}  // namespace

// Writes a summary of annotations, calls, and errors.
bool WriteSummaryFile(const base::FilePath& filepath,
                      const std::vector<AnnotationInstance>& annotations,
                      const std::vector<CallInstance>& calls,
                      const std::vector<AuditorResult>& errors) {
  std::string report;
  std::vector<std::string> items;

  report = "[Errors]\n";
  for (const auto& error : errors)
    items.push_back(error.ToText());
  std::sort(items.begin(), items.end());
  for (const std::string& item : items)
    report += item + "\n";

  report += "\n[Annotations]\n";
  items.clear();
  for (const auto& instance : annotations) {
    std::string serialized;
    google::protobuf::TextFormat::PrintToString(instance.proto, &serialized);
    items.push_back(serialized +
                    "\n----------------------------------------\n");
  }
  std::sort(items.begin(), items.end());
  for (const std::string& item : items)
    report += item;

  report += "\n[Calls]\n";
  items.clear();
  for (const auto& instance : calls) {
    items.push_back(base::StringPrintf(
        "File:%s:%i\nFunction:%s\nAnnotated: %i\n", instance.file_path.c_str(),
        instance.line_number, instance.function_name.c_str(),
        instance.is_annotated));
  }
  std::sort(items.begin(), items.end());
  for (const std::string& item : items)
    report += item;

  return base::WriteFile(filepath, report.c_str(), report.length()) != -1;
}

// Changes double quotations to single quotations, and adds quotations if the
// text includes end of lines or tabs.
std::string UpdateTextForTSV(std::string text) {
  base::ReplaceChars(text, "\"", "'", &text);
  if (text.find('\n') != std::string::npos ||
      text.find('\t') != std::string::npos)
    return base::StringPrintf("\"%s\"", text.c_str());
  return text;
}

ExtractorBackend GetExtractorBackend(const std::string& backend_switch) {
  if (backend_switch == "clang_tool")
    return ExtractorBackend::CLANG_TOOL;
  if (backend_switch.empty() || backend_switch == "python_script")
    return ExtractorBackend::PYTHON_SCRIPT;
  return ExtractorBackend::INVALID;
}

// TODO(rhalavati): Update this function to extract the policy name and value
// directly from the ChromeSettingsProto object (gen/components/policy/proto/
// chrome_settings.proto). Since ChromeSettingsProto has over 300+
// implementations, the required output is now extracted from debug output as
// the debug output has the following format:
//   POLICY_NAME {
//    ...
//   POLICY_NAME: POLICY_VALUE (policy value may extend to several lines.)
//   }
std::string PolicyToText(std::string debug_string) {
  std::vector<std::string> lines = base::SplitString(
      debug_string, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK(lines.size() && lines[0].length() > 3);
  DCHECK_EQ(lines[0].substr(lines[0].length() - 2, 2), " {");
  // Get the title, remove the open curly bracket.
  std::string title = lines[0].substr(0, lines[0].length() - 2);
  std::string output;
  // Find the first line that has the title in it, keep adding all next lines
  // to have the full value description.
  for (unsigned int i = 1; i < lines.size(); i++) {
    if (!output.empty()) {
      output += lines[i] + " ";
    } else if (lines[i].find(title) != std::string::npos) {
      output += lines[i] + " ";
    }
  }

  // Trim trailing spaces and at most one curly bracket.
  base::TrimString(output, " ", &output);
  DCHECK(!output.empty());
  if (output[output.length() - 1] == '}')
    output.pop_back();
  base::TrimString(output, " ", &output);

  return output;
}

// Writes a TSV file of all annotations and their content.
bool WriteAnnotationsFile(const base::FilePath& filepath,
                          const std::vector<AnnotationInstance>& annotations,
                          const std::vector<std::string>& missing_ids) {
  std::vector<std::string> lines;
  std::string title =
      "Unique ID\tLast Update\tSender\tDescription\tTrigger\tData\t"
      "Destination\tCookies Allowed\tCookies Store\tSetting\tChrome Policy\t"
      "Comments\tSource File\tID Hash Code\tContent Hash Code";

  for (auto& instance : annotations) {
    if (instance.type != AnnotationInstance::Type::ANNOTATION_COMPLETE)
      continue;
    // Unique ID
    std::string line = instance.proto.unique_id();

    // Place holder for Last Update Date, will be updated in the scripts.
    line += "\t";

    // Semantics.
    const auto semantics = instance.proto.semantics();
    line += base::StringPrintf("\t%s", semantics.sender().c_str());
    line += base::StringPrintf(
        "\t%s", UpdateTextForTSV(semantics.description()).c_str());
    line += base::StringPrintf("\t%s",
                               UpdateTextForTSV(semantics.trigger()).c_str());
    line +=
        base::StringPrintf("\t%s", UpdateTextForTSV(semantics.data()).c_str());
    switch (semantics.destination()) {
      case traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_WEBSITE:
        line += "\tWebsite";
        break;
      case traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_GOOGLE_OWNED_SERVICE:
        line += "\tGoogle";
        break;
      case traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_LOCAL:
        line += "\tLocal";
        break;
      case traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_OTHER:
        if (!semantics.destination_other().empty()) {
          line += "\t";
          line += UpdateTextForTSV(base::StringPrintf(
              "Other: %s", semantics.destination_other().c_str()));
        } else {
          line += "\tOther";
        }
        break;

      default:
        NOTREACHED();
        line += "\tInvalid value";
    }

    // Policy.
    const auto policy = instance.proto.policy();
    line +=
        policy.cookies_allowed() ==
                traffic_annotation::
                    NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES
            ? "\tYes"
            : "\tNo";
    line += base::StringPrintf(
        "\t%s", UpdateTextForTSV(policy.cookies_store()).c_str());
    line +=
        base::StringPrintf("\t%s", UpdateTextForTSV(policy.setting()).c_str());

    // Chrome Policies.
    std::string policies_text;
    if (policy.chrome_policy_size()) {
      for (int i = 0; i < policy.chrome_policy_size(); i++) {
        if (i)
          policies_text += "\n";
        policies_text += PolicyToText(policy.chrome_policy(i).DebugString());
      }
    } else {
      policies_text = policy.policy_exception_justification();
    }
    line += base::StringPrintf("\t%s", UpdateTextForTSV(policies_text).c_str());

    // Comments.
    line += "\t" + UpdateTextForTSV(instance.proto.comments());

    // Source.
    const auto source = instance.proto.source();
    line += base::StringPrintf("\t%s%s?l=%i", kCodeSearchLink.c_str(),
                               source.file().c_str(), source.line());

    // ID Hash code.
    line += base::StringPrintf("\t%i", instance.unique_id_hash_code);

    // Content Hash code.
    line += base::StringPrintf("\t%i", instance.GetContentHashCode());

    lines.push_back(line);
  }

  // Add missing annotations.
  int columns = std::count(title.begin(), title.end(), '\t');
  std::string tabs(columns, '\t');

  for (const std::string& id : missing_ids) {
    lines.push_back(id + tabs);
  }

  std::sort(lines.begin(), lines.end());
  lines.insert(lines.begin(), title);
  std::string report;
  for (const std::string& line : lines) {
    report += line + "\n";
  }

  return base::WriteFile(filepath, report.c_str(), report.length()) != -1;
}

#if defined(OS_WIN)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
  printf(
      "Starting traffic annotation auditor. This may take from a few "
      "minutes to a few hours based on the scope of the test.\n");

  // Parse switches.
  base::CommandLine command_line = base::CommandLine(argc, argv);
  if (command_line.HasSwitch("help") || command_line.HasSwitch("h") ||
      argc == 1) {
    printf("%s", HELP_TEXT);
    return 1;
  }

  base::FilePath build_path = command_line.GetSwitchValuePath("build-path");
  base::FilePath source_path = command_line.GetSwitchValuePath("source-path");
  base::FilePath tool_path = command_line.GetSwitchValuePath("tool-path");
  base::FilePath extractor_output =
      command_line.GetSwitchValuePath("extractor-output");
  base::FilePath extractor_input =
      command_line.GetSwitchValuePath("extractor-input");
  base::FilePath errors_file = command_line.GetSwitchValuePath("errors-file");
  bool filter_files = !command_line.HasSwitch("no-filtering");
  bool all_files = command_line.HasSwitch("all-files");
  bool test_only = command_line.HasSwitch("test-only");
  bool no_missing_error = command_line.HasSwitch("no-missing-error");
  base::FilePath summary_file = command_line.GetSwitchValuePath("summary-file");
  base::FilePath annotations_file =
      command_line.GetSwitchValuePath("annotations-file");
  std::vector<std::string> path_filters;
  int outputs_limit = 0;
  if (command_line.HasSwitch("limit")) {
    if (!base::StringToInt(command_line.GetSwitchValueNative("limit"),
                           &outputs_limit) ||
        outputs_limit < 0) {
      LOG(ERROR)
          << "The value for 'limit' switch should be a positive integer.";

      // This error is always enforced, as it is a commandline switch.
      return 1;
    }
  }

  // If 'error-resilient' switch is provided, 0 will be returned in case of
  // operational errors, otherwise 1.
  bool error_resilient = command_line.HasSwitch("error-resilient");
  int error_value = error_resilient ? 0 : 1;

#if defined(OS_WIN)
  for (const auto& path : command_line.GetArgs()) {
    std::string repaired_path(base::UTF16ToASCII(path));
    base::ReplaceChars(repaired_path, "\\", "/", &repaired_path);
    path_filters.push_back(repaired_path);
  }
#else
  path_filters = command_line.GetArgs();
#endif

  // If tool path is not specified, assume it is in the same path as this
  // executable.
  if (tool_path.empty())
    tool_path = command_line.GetProgram().DirName();

  // Get build directory, if it is empty issue an error.
  if (build_path.empty()) {
    LOG(ERROR)
        << "You must specify a compiled build directory to run the auditor.\n";

    // This error is always enforced, as it is a commandline switch.
    return 1;
  }

  // If source path is not provided, guess it using build path.
  if (source_path.empty()) {
    source_path = build_path.Append(base::FilePath::kParentDirectory)
                      .Append(base::FilePath::kParentDirectory);
  }

  TrafficAnnotationAuditor auditor(source_path, build_path, tool_path,
                                   path_filters);

  // Extract annotations.
  if (extractor_input.empty()) {
    std::string backend_switch =
        command_line.GetSwitchValueASCII("extractor-backend");
    ExtractorBackend backend = GetExtractorBackend(backend_switch);
    if (backend == ExtractorBackend::INVALID) {
      LOG(ERROR) << "Unrecognized extractor backend '" << backend_switch << "'";
      return error_value;
    }

    // If we're using the Python backend, it's fast enough that we can ignore
    // any path filters when we say we want to audit everything.
    if (backend == ExtractorBackend::PYTHON_SCRIPT &&
        (!filter_files || all_files)) {
      LOG(WARNING) << "The path_filters input is being ignored.";
      auditor.ClearPathFilters();
    }

    if (!auditor.RunExtractor(backend, filter_files, all_files,
                              !error_resilient, errors_file)) {
      LOG(ERROR) << "Failed to run clang tool.";
      return error_value;
    }

    // Write extractor output if requested.
    if (!extractor_output.empty()) {
      std::string raw_output = auditor.extractor_raw_output();
      base::WriteFile(extractor_output, raw_output.c_str(),
                      raw_output.length());
    }
  } else {
    std::string raw_output;
    if (!base::ReadFileToString(extractor_input, &raw_output)) {
      LOG(ERROR) << "Could not read input file: "
                 << extractor_input.value().c_str();
      return error_value;
    } else {
      auditor.set_extractor_raw_output(raw_output);
    }
  }

  // Process extractor output.
  if (!auditor.ParseClangToolRawOutput())
    return error_value;

  // Perform checks.
  if (!auditor.RunAllChecks(test_only)) {
    LOG(ERROR) << "Running checks failed.";
    return error_value;
  }

  // Write the summary file.
  if (!summary_file.empty() &&
      !WriteSummaryFile(summary_file, auditor.extracted_annotations(),
                        auditor.extracted_calls(), auditor.errors())) {
    LOG(ERROR) << "Could not write summary file.";
    return error_value;
  }

  // Write annotations TSV file.
  if (!annotations_file.empty()) {
    std::vector<std::string> missing_ids;
    if (!auditor.exporter().GetOtherPlatformsAnnotationIDs(&missing_ids) ||
        !WriteAnnotationsFile(annotations_file, auditor.extracted_annotations(),
                              missing_ids)) {
      LOG(ERROR) << "Could not write TSV file.";
      return error_value;
    }
  }

  const std::vector<AuditorResult>& raw_errors = auditor.errors();

  std::vector<AuditorResult> errors;
  std::vector<AuditorResult> warnings;
  std::set<AuditorResult::Type> warning_types;

  if (no_missing_error) {
    warning_types.insert(AuditorResult::Type::ERROR_MISSING_ANNOTATION);
    warning_types.insert(AuditorResult::Type::ERROR_NO_ANNOTATION);
  }

  for (const AuditorResult& result : raw_errors) {
    if (base::Contains(warning_types, result.type()))
      warnings.push_back(result);
    else
      errors.push_back(result);
  }

  // Update annotations.xml if everything else is OK and the auditor is not
  // run in test-only mode.
  if (errors.empty() && !test_only) {
    if (!auditor.exporter().SaveAnnotationsXML()) {
      LOG(ERROR) << "Could not update annotations XML.";
      return error_value;
    }
  }

  // Dump Warnings and Errors to stdout.
  int remaining_outputs =
      outputs_limit ? outputs_limit
                    : static_cast<int>(errors.size() + warnings.size());
  if (errors.size()) {
    printf("[Errors]\n");
    for (int i = 0; i < static_cast<int>(errors.size()) && remaining_outputs;
         i++, remaining_outputs--) {
      printf("  (%i)\t%s\n", i + 1, errors[i].ToText().c_str());
    }
  }
  if (warnings.size() && remaining_outputs) {
    printf("[Warnings]\n");
    for (int i = 0; i < static_cast<int>(warnings.size()) && remaining_outputs;
         i++, remaining_outputs--) {
      printf("  (%i)\t%s\n", i + 1, warnings[i].ToText().c_str());
    }
  }
  if (warnings.empty() && errors.empty())
    printf("Traffic annotations are all OK.\n");

  return static_cast<int>(errors.size());
}
