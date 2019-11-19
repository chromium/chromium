// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"

#include <stdio.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/re2/src/re2/re2.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_file_filter.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_id_checker.h"

namespace {

// Recursively compute the hash code of the given string as in:
// "net/traffic_annotation/network_traffic_annotation.h"
uint32_t recursive_hash(const char* str, int N) {
  if (N == 1)
    return static_cast<uint32_t>(str[0]);
  else
    return (recursive_hash(str, N - 1) * 31 + str[N - 1]) % 138003713;
}

std::map<int, std::string> kReservedAnnotations = {
    {TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code, "test"},
    {PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code, "test_partial"},
    {MISSING_TRAFFIC_ANNOTATION.unique_id_hash_code, "missing"}};

struct AnnotationID {
  // Two ids can be the same in the following cases:
  // 1- One is extra id of a partial annotation, and the other is either the
  //    unique id of a completing annotation, or extra id of a partial or
  //    branched completing annotation
  // 2- Both are extra ids of branched completing annotations.
  // The following Type value facilitate these checks.
  enum class Type { kPatrialExtra, kCompletingMain, kBranchedExtra, kOther };
  Type type;
  std::string text;
  int hash;
  AnnotationInstance* instance;
};

const std::string kBlockTypes[] = {"ASSIGNMENT", "ANNOTATION", "CALL"};

const base::FilePath kSafeListPath =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("auditor"))
        .Append(FILE_PATH_LITERAL("safe_list.txt"));

const base::FilePath kClangToolSwitchesPath =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("auditor"))
        .Append(FILE_PATH_LITERAL("traffic_annotation_extractor_switches.txt"));

// The folder that includes the latest Clang built-in library. Inside this
// folder, there should be another folder with version number, like
// '.../lib/clang/6.0.0', which would be passed to the clang tool.
const base::FilePath kClangLibraryPath =
    base::FilePath(FILE_PATH_LITERAL("third_party"))
        .Append(FILE_PATH_LITERAL("llvm-build"))
        .Append(FILE_PATH_LITERAL("Release+Asserts"))
        .Append(FILE_PATH_LITERAL("lib"))
        .Append(FILE_PATH_LITERAL("clang"));

const base::FilePath kRunToolScript =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("clang"))
        .Append(FILE_PATH_LITERAL("scripts"))
        .Append(FILE_PATH_LITERAL("run_tool.py"));

const base::FilePath kExtractorScript =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("scripts"))
        .Append(FILE_PATH_LITERAL("extractor.py"));

// Checks if the list of |path_filters| include the given |file_path|, or there
// are path filters which are a folder (don't have a '.' in their name), and
// match the file name.
// TODO(https://crbug.com/690323): Update to a more general policy.
bool PathFiltersMatch(const std::vector<std::string>& path_filters,
                      const std::string file_path) {
  if (base::Contains(path_filters, file_path))
    return true;
  for (const std::string& path_filter : path_filters) {
    if (path_filter.find(".") == std::string::npos &&
        file_path.substr(0, path_filter.length()) == path_filter) {
      return true;
    }
  }
  return false;
}

// If normalized |file_path| starts with |base_directory|, returns the
// relative path to |file_path|, otherwise the original |file_path| is returned.
std::string MakeRelativePath(const base::FilePath& base_directory,
                             const std::string& file_path) {
  DCHECK(base_directory.IsAbsolute());

#if defined(OS_WIN)
  base::FilePath converted_file_path = base::FilePath(
      base::FilePath::StringPieceType((base::UTF8ToWide(file_path))));
#else
  base::FilePath converted_file_path(file_path);
#endif
  converted_file_path = base::MakeAbsoluteFilePath(converted_file_path);
  base::FilePath normalized_path;
  if (base::NormalizeFilePath(converted_file_path, &normalized_path) &&
      normalized_path.IsAbsolute()) {
    normalized_path = normalized_path.NormalizePathSeparatorsTo('/');
    std::string file_str = normalized_path.MaybeAsASCII();
    std::string base_str = base_directory.MaybeAsASCII();
    if (file_str.find(base_str) == 0) {
      return file_str.substr(base_str.length() + 1,
                             file_str.length() - base_str.length() - 1);
    }
  }
  return file_path;
}

}  // namespace

TrafficAnnotationAuditor::TrafficAnnotationAuditor(
    const base::FilePath& source_path,
    const base::FilePath& build_path,
    const base::FilePath& clang_tool_path,
    const std::vector<std::string>& path_filters)
    : source_path_(source_path),
      build_path_(build_path),
      clang_tool_path_(clang_tool_path),
      path_filters_(path_filters),
      exporter_(source_path),
      safe_list_loaded_(false) {
  DCHECK(!source_path.empty());
  DCHECK(!build_path.empty());
  DCHECK(!clang_tool_path.empty());

  // Get absolute source path.
  base::FilePath original_path;
  base::GetCurrentDirectory(&original_path);
  base::SetCurrentDirectory(source_path_);
  base::GetCurrentDirectory(&absolute_source_path_);
  base::SetCurrentDirectory(original_path);
  absolute_source_path_ = absolute_source_path_.NormalizePathSeparatorsTo('/');
  DCHECK(absolute_source_path_.IsAbsolute());

  base::FilePath switches_file =
      base::MakeAbsoluteFilePath(source_path_.Append(kClangToolSwitchesPath));
  std::string file_content;
  if (base::ReadFileToString(switches_file, &file_content)) {
    clang_tool_switches_ = base::SplitString(
        file_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  } else {
    LOG(ERROR) << "Could not read " << kClangToolSwitchesPath;
  }
}

TrafficAnnotationAuditor::~TrafficAnnotationAuditor() = default;

// static
int TrafficAnnotationAuditor::ComputeHashValue(const std::string& unique_id) {
  return unique_id.length() ? static_cast<int>(recursive_hash(
                                  unique_id.c_str(), unique_id.length()))
                            : -1;
}

base::FilePath TrafficAnnotationAuditor::GetClangLibraryPath() {
  return base::FileEnumerator(source_path_.Append(kClangLibraryPath), false,
                              base::FileEnumerator::DIRECTORIES)
      .Next();
}

bool TrafficAnnotationAuditor::RunExtractor(
    ExtractorBackend backend,
    bool filter_files_based_on_heuristics,
    bool use_compile_commands,
    bool rerun_on_errors,
    const base::FilePath& errors_file) {
  DCHECK(backend == ExtractorBackend::CLANG_TOOL ||
         backend == ExtractorBackend::PYTHON_SCRIPT);

  if (!safe_list_loaded_ && !LoadSafeList())
    return false;

  // Get list of files/folders to process.
  std::vector<std::string> file_paths;
  GenerateFilesListForClangTool(backend, filter_files_based_on_heuristics,
                                use_compile_commands, &file_paths);
  if (file_paths.empty())
    return true;

  // Create a file to pass options to the clang tool running script.
  base::FilePath options_filepath;
  if (!base::CreateTemporaryFile(&options_filepath)) {
    LOG(ERROR) << "Could not create temporary options file.";
    return false;
  }
  FILE* options_file = base::OpenFile(options_filepath, "wt");
  if (!options_file) {
    LOG(ERROR) << "Could not open temporary options file.";
    return false;
  }

  // Write some options to the file, which depends on the backend used.
  if (backend == ExtractorBackend::CLANG_TOOL)
    WriteClangToolOptions(options_file, use_compile_commands);
  else if (backend == ExtractorBackend::PYTHON_SCRIPT)
    WritePythonScriptOptions(options_file);

  // Write the file paths regardless of backend.
  for (const std::string& file_path : file_paths)
    fprintf(options_file, "%s ", file_path.c_str());

  base::CloseFile(options_file);

  const base::FilePath& script_path =
      (backend == ExtractorBackend::CLANG_TOOL ? kRunToolScript
                                               : kExtractorScript);
  base::CommandLine cmdline(
      base::MakeAbsoluteFilePath(source_path_.Append(script_path)));
#if defined(OS_WIN)
  cmdline.PrependWrapper(L"python");
#endif
  cmdline.AppendArg(base::StringPrintf(
      "--options-file=%s", options_filepath.MaybeAsASCII().c_str()));

  // Change current folder to source before running the command as it expects to
  // be run from there.
  base::FilePath original_path;
  base::GetCurrentDirectory(&original_path);
  base::SetCurrentDirectory(source_path_);
  bool result = base::GetAppOutput(cmdline, &extractor_raw_output_);

  // If the extractor had no output, it means that the script running it could
  // not perform the task.
  if (extractor_raw_output_.empty()) {
    result = false;
  } else if (backend == ExtractorBackend::CLANG_TOOL && !result) {
    // If clang tool had errors but also returned results, the errors can be
    // ignored as we do not separate platform specific files here and processing
    // them fails. This is a post-build test and if there exists any actual
    // compile error, it should be noted when the code is built.
    printf("WARNING: Ignoring clang tool's returned errors.\n");
    result = true;
  }

  if (!result) {
    if (backend == ExtractorBackend::CLANG_TOOL && use_compile_commands &&
        !extractor_raw_output_.empty()) {
      printf(
          "\nWARNING: Ignoring clang tool error as it is called using "
          "compile_commands.json which will result in processing some "
          "library files that clang cannot process.\n");
      result = true;
    } else {
      std::string tool_errors;
      std::string options_file_text;

      if (rerun_on_errors)
        base::GetAppOutputAndError(cmdline, &tool_errors);
      else
        tool_errors = "Not Available.";

      if (!base::ReadFileToString(options_filepath, &options_file_text))
        options_file_text = "Could not read options file.";

      std::string error_message = base::StringPrintf(
          "Calling clang tool returned false from %s\nCommandline: %s\n\n"
          "Returned output: %s\n\nPartial options file: %s\n",
          source_path_.MaybeAsASCII().c_str(),
#if defined(OS_WIN)
          base::UTF16ToASCII(cmdline.GetCommandLineString()).c_str(),
#else
          cmdline.GetCommandLineString().c_str(),
#endif
          tool_errors.c_str(), options_file_text.substr(0, 1024).c_str());

      if (errors_file.empty()) {
        LOG(ERROR) << error_message;
      } else {
        if (base::WriteFile(errors_file, error_message.c_str(),
                            error_message.length()) == -1) {
          LOG(ERROR) << "Writing error message to file failed:\n"
                     << error_message;
        }
      }
    }
  }

  base::SetCurrentDirectory(original_path);
  base::DeleteFile(options_filepath, false);

  return result;
}

void TrafficAnnotationAuditor::WriteClangToolOptions(
    FILE* options_file,
    bool use_compile_commands) {
  // As the checked out clang tool may be in a directory different from the
  // default one (third_party/llvm-build/Release+Asserts/bin), its path and
  // clang's library folder should be passed to the run_tool.py script.
  fprintf(
      options_file,
      "--generate-compdb --tool=traffic_annotation_extractor -p=%s "
      "--tool-path=%s "
      "--tool-arg=--extra-arg=-resource-dir=%s ",
      build_path_.MaybeAsASCII().c_str(),
      base::MakeAbsoluteFilePath(clang_tool_path_).MaybeAsASCII().c_str(),
      base::MakeAbsoluteFilePath(GetClangLibraryPath()).MaybeAsASCII().c_str());

  for (const std::string& item : clang_tool_switches_)
    fprintf(options_file, "--tool-arg=--extra-arg=%s ", item.c_str());

  if (use_compile_commands)
    fprintf(options_file, "--all ");
}

void TrafficAnnotationAuditor::WritePythonScriptOptions(FILE* options_file) {
  fprintf(options_file, "--generate-compdb --build-path=%s ",
          build_path_.MaybeAsASCII().c_str());
}

void TrafficAnnotationAuditor::GenerateFilesListForClangTool(
    ExtractorBackend backend,
    bool filter_files_based_on_heuristics,
    bool use_compile_commands,
    std::vector<std::string>* file_paths) {
  TrafficAnnotationFileFilter filter;

  // When using the Clang tool backend and |use_compile_commands| is requested
  // or |filter_files_based_on_heuristics| is false, we pass all given file
  // paths to the running script and the files in the safe list will be later
  // removed from the results. The Python tool requires a good list of file
  // paths and cannot implement the same logic.
  if (backend == ExtractorBackend::CLANG_TOOL &&
      (!filter_files_based_on_heuristics || use_compile_commands)) {
    if (path_filters_.empty()) {
      // If no path filter is specified, return current location. The clang tool
      // will be run from the repository 'src' folder and hence this will point
      // to repository root.
      file_paths->push_back("./");
    } else {
      *file_paths = path_filters_;
    }
    return;
  }

  // If no path filter is provided, get all relevant files, except the safe
  // listed ones.
  if (path_filters_.empty()) {
    filter.GetRelevantFiles(
        source_path_,
        safe_list_[static_cast<int>(AuditorException::ExceptionType::ALL)], "",
        file_paths);
    return;
  }

  base::FilePath original_path;
  base::GetCurrentDirectory(&original_path);
  base::SetCurrentDirectory(source_path_);

  bool possibly_deleted_files = false;
  for (const auto& path_filter : path_filters_) {
#if defined(OS_WIN)
    base::FilePath path = base::FilePath(
        base::FilePath::StringPieceType((base::UTF8ToWide(path_filter))));
#else
    base::FilePath path = base::FilePath(path_filter);
#endif

    // If path filter is a directory, add its relevent, not safe-listed
    // contents.
    if (base::DirectoryExists(path)) {
      filter.GetRelevantFiles(
          source_path_,
          safe_list_[static_cast<int>(AuditorException::ExceptionType::ALL)],
          path_filter, file_paths);
    } else {
      // Add the file if it exists and is a relevant file which is not
      // safe-listed.
      if (base::PathExists(path)) {
        if (!TrafficAnnotationAuditor::IsSafeListed(
                path_filter, AuditorException::ExceptionType::ALL) &&
            filter.IsFileRelevant(path_filter)) {
          file_paths->push_back(path_filter);
        }
      } else {
        possibly_deleted_files = true;
      }
    }
  }

  base::SetCurrentDirectory(original_path);
}

bool TrafficAnnotationAuditor::IsSafeListed(
    const std::string& file_path,
    AuditorException::ExceptionType exception_type) {
  if (!safe_list_loaded_ && !LoadSafeList())
    return false;
  const std::vector<std::string>& safe_list =
      safe_list_[static_cast<int>(exception_type)];

  for (const std::string& ignore_pattern : safe_list) {
    if (re2::RE2::FullMatch(file_path, ignore_pattern))
      return true;
  }

  // If the given filepath did not match the rules with the specified type,
  // check it with rules of type 'ALL' as well.
  if (exception_type != AuditorException::ExceptionType::ALL)
    return IsSafeListed(file_path, AuditorException::ExceptionType::ALL);
  return false;
}

bool TrafficAnnotationAuditor::ParseClangToolRawOutput() {
  if (!safe_list_loaded_ && !LoadSafeList())
    return false;
  // Remove possible carriage return characters before splitting lines.
  std::string temp_string;
  base::RemoveChars(extractor_raw_output_, "\r", &temp_string);
  std::vector<std::string> lines = base::SplitString(
      temp_string, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (unsigned int current = 0; current < lines.size(); current++) {
    // All blocks reported by clang tool start with '====', so we can ignore
    // all lines that do not start with a '='.
    if (lines[current].empty() || lines[current][0] != '=')
      continue;

    std::string block_type;
    std::string end_marker;
    for (const std::string& item : kBlockTypes) {
      if (lines[current] ==
          base::StringPrintf("==== NEW %s ====", item.c_str())) {
        end_marker = base::StringPrintf("==== %s ENDS ====", item.c_str());
        block_type = item;
        break;
      }
    }

    // If not starting a valid block, ignore the line.
    if (block_type.empty())
      continue;

    // Get the block.
    current++;
    unsigned int end_line = current;
    while (end_line < lines.size() && lines[end_line] != end_marker)
      end_line++;
    if (end_line == lines.size()) {
      LOG(ERROR) << "Block starting at line " << current
                 << " is not ended by the appropriate tag.";
      return false;
    }

    // Deserialize and handle errors.
    AuditorResult result(AuditorResult::Type::RESULT_OK);

    if (block_type == "ANNOTATION") {
      AnnotationInstance new_annotation;
      result = new_annotation.Deserialize(lines, current, end_line);
      std::string file_path = result.IsOK()
                                  ? new_annotation.proto.source().file()
                                  : result.file_path();
      file_path = MakeRelativePath(absolute_source_path_, file_path);
      if (IsSafeListed(file_path, AuditorException::ExceptionType::ALL))
        result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
      switch (result.type()) {
        case AuditorResult::Type::RESULT_OK:
          new_annotation.proto.mutable_source()->set_file(file_path);
          extracted_annotations_.push_back(new_annotation);
          break;
        case AuditorResult::Type::ERROR_MISSING_TAG_USED:
          if (IsSafeListed(file_path, AuditorException::ExceptionType::MISSING))
            result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
          break;
        case AuditorResult::Type::ERROR_TEST_ANNOTATION:
          if (IsSafeListed(file_path,
                           AuditorException::ExceptionType::TEST_ANNOTATION)) {
            result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
          }
          break;
        case AuditorResult::Type::ERROR_MUTABLE_TAG:
          if (IsSafeListed(file_path,
                           AuditorException::ExceptionType::MUTABLE_TAG)) {
            result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
          }
          break;
        default:
          break;
      }
    } else if (block_type == "CALL") {
      CallInstance new_call;
      result = new_call.Deserialize(lines, current, end_line);
      new_call.file_path =
          MakeRelativePath(absolute_source_path_, new_call.file_path);
      if (IsSafeListed(new_call.file_path,
                       AuditorException::ExceptionType::ALL)) {
        result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
      }
      if (result.IsOK())
        extracted_calls_.push_back(new_call);
    } else if (block_type == "ASSIGNMENT") {
      AssignmentInstance new_assignment;
      result = new_assignment.Deserialize(lines, current, end_line);
      new_assignment.file_path =
          MakeRelativePath(absolute_source_path_, new_assignment.file_path);
      if (IsSafeListed(new_assignment.file_path,
                       AuditorException::ExceptionType::DIRECT_ASSIGNMENT)) {
        result = AuditorResult(AuditorResult::Type::RESULT_IGNORE);
      } else if (result.IsOK()) {
        result = AuditorResult(AuditorResult::Type::ERROR_DIRECT_ASSIGNMENT,
                               std::string(), new_assignment.file_path,
                               new_assignment.line_number);
      }
    } else {
      NOTREACHED();
    }

    switch (result.type()) {
      case AuditorResult::Type::RESULT_OK:
      case AuditorResult::Type::RESULT_IGNORE:
        break;
      case AuditorResult::Type::ERROR_FATAL: {
        LOG(ERROR) << "Aborting after line " << current
                   << " because: " << result.ToText().c_str();
        return false;
      }
      default:
        if (!IsSafeListed(result.file_path(),
                          AuditorException::ExceptionType::ALL)) {
          errors_.push_back(result);
        }
    }

    current = end_line;
  }  // for

  return true;
}

bool TrafficAnnotationAuditor::LoadSafeList() {
  base::FilePath safe_list_file =
      base::MakeAbsoluteFilePath(source_path_.Append(kSafeListPath));

  std::string file_content;
  if (!base::ReadFileToString(safe_list_file, &file_content)) {
    LOG(ERROR) << "Could not read " << kSafeListPath.MaybeAsASCII();
    return false;
  }

  base::RemoveChars(file_content, "\r", &file_content);
  std::vector<std::string> lines = base::SplitString(
      file_content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (const std::string& line : lines) {
    // Ignore comments and empty lines.
    if (!line.length() || line[0] == '#')
      continue;

    std::vector<std::string> tokens = base::SplitString(
        line, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    // Expect a type and at least one value in each line.
    if (tokens.size() < 2) {
      LOG(ERROR) << "Unexpected syntax in safe_list.txt, line: " << line;
      return false;
    }

    AuditorException::ExceptionType exception_type;
    if (!AuditorException::TypeFromString(tokens[0], &exception_type)) {
      LOG(ERROR) << "Unexpected type in safe_list.txt line: " << line;
      return false;
    }
    for (unsigned i = 1; i < tokens.size(); i++) {
      // Convert the rest of the line into re2 patterns, making dots as fixed
      // characters and asterisks as wildcards.
      // Note that all file paths are converted to Linux style before checking.
      if (!base::ContainsOnlyChars(
              base::ToLowerASCII(tokens[i]),
              "0123456789_abcdefghijklmnopqrstuvwxyz.*/:@")) {
        LOG(ERROR) << "Unexpected character in safe_list.txt token: "
                   << tokens[i];
        return false;
      }
      std::string pattern;
      base::ReplaceChars(tokens[i], ".", "[.]", &pattern);
      base::ReplaceChars(pattern, "*", ".*", &pattern);
      safe_list_[static_cast<int>(exception_type)].push_back(pattern);
    }
  }

  safe_list_loaded_ = true;
  return true;
}

// static
const std::map<int, std::string>&
TrafficAnnotationAuditor::GetReservedIDsMap() {
  return kReservedAnnotations;
}

// static
std::set<int> TrafficAnnotationAuditor::GetReservedIDsSet() {
  std::set<int> reserved_ids;
  for (const auto& item : kReservedAnnotations)
    reserved_ids.insert(item.first);
  return reserved_ids;
}

void TrafficAnnotationAuditor::CheckAllRequiredFunctionsAreAnnotated() {
  for (const CallInstance& call : extracted_calls_) {
    if (!call.is_annotated && !CheckIfCallCanBeUnannotated(call)) {
      errors_.push_back(
          AuditorResult(AuditorResult::Type::ERROR_MISSING_ANNOTATION,
                        call.function_name, call.file_path, call.line_number));
    }
  }
}

bool TrafficAnnotationAuditor::CheckIfCallCanBeUnannotated(
    const CallInstance& call) {
  if (IsSafeListed(call.file_path, AuditorException::ExceptionType::MISSING))
    return true;

  // Unittests should be all annotated. Although this can be detected using gn,
  // doing that would be very slow. The alternative solution would be to bypass
  // every file including test or unittest, but in this case there might be some
  // ambiguity in what should be annotated and what not.
  if (call.file_path.find("unittest") != std::string::npos)
    return false;

  // Already checked?
  if (base::Contains(checked_dependencies_, call.file_path))
    return checked_dependencies_[call.file_path];

  std::string gn_output;
  if (gn_file_for_test_.empty()) {
    // Check if the file including this function is part of Chrome build.
    const base::CommandLine::CharType* args[] = {
#if defined(OS_WIN)
      FILE_PATH_LITERAL("buildtools/win/gn.exe"),
#elif defined(OS_MACOSX)
      FILE_PATH_LITERAL("buildtools/mac/gn"),
#elif defined(OS_LINUX)
      FILE_PATH_LITERAL("buildtools/linux64/gn"),
#else
      // Fallback to using PATH to find gn.
      FILE_PATH_LITERAL("gn"),
#endif
      FILE_PATH_LITERAL("refs"),
      FILE_PATH_LITERAL("--all")
    };

    base::CommandLine cmdline(3, args);
    cmdline.AppendArgPath(build_path_);
    cmdline.AppendArg(call.file_path);

    base::FilePath original_path;
    base::GetCurrentDirectory(&original_path);
    base::SetCurrentDirectory(source_path_);
    if (!base::GetAppOutput(cmdline, &gn_output)) {
      LOG(ERROR) << "Could not run gn to get dependencies.";
      gn_output.clear();
    }
    base::SetCurrentDirectory(original_path);
  } else {
    if (!base::ReadFileToString(gn_file_for_test_, &gn_output)) {
      LOG(ERROR) << "Could not load mock gn output file from "
                 << gn_file_for_test_.MaybeAsASCII().c_str();
      gn_output.clear();
    }
  }

  checked_dependencies_[call.file_path] =
      gn_output.length() &&
      gn_output.find("//chrome:chrome") == std::string::npos;

  return checked_dependencies_[call.file_path];
}

void TrafficAnnotationAuditor::CheckAnnotationsContents() {
  std::vector<AnnotationInstance*> partial_annotations;
  std::vector<AnnotationInstance*> completing_annotations;
  std::vector<AnnotationInstance> new_annotations;

  // Process complete annotations and separate the others.
  for (AnnotationInstance& instance : extracted_annotations_) {
    switch (instance.type) {
      case AnnotationInstance::Type::ANNOTATION_COMPLETE: {
        // Instances loaded from archive are already checked before archiving.
        if (instance.is_loaded_from_archive)
          continue;
        AuditorResult result = instance.IsComplete();
        if (result.IsOK())
          result = instance.IsConsistent();
        if (!result.IsOK())
          errors_.push_back(result);
        break;
      }
      case AnnotationInstance::Type::ANNOTATION_PARTIAL:
        partial_annotations.push_back(&instance);
        break;
      default:
        completing_annotations.push_back(&instance);
    }
  }

  std::set<AnnotationInstance*> used_completing_annotations;

  for (AnnotationInstance* partial : partial_annotations) {
    bool found_a_pair = false;
    for (AnnotationInstance* completing : completing_annotations) {
      if (partial->IsCompletableWith(*completing)) {
        found_a_pair = true;
        used_completing_annotations.insert(completing);

        // Instances loaded from archive are already checked before archiving.
        if (partial->is_loaded_from_archive &&
            completing->is_loaded_from_archive) {
          break;
        }

        AnnotationInstance completed;
        AuditorResult result =
            partial->CreateCompleteAnnotation(*completing, &completed);

        if (result.IsOK())
          result = completed.IsComplete();

        if (result.IsOK())
          result = completed.IsConsistent();

        if (result.IsOK()) {
          new_annotations.push_back(completed);
        } else {
          result = AuditorResult(AuditorResult::Type::ERROR_MERGE_FAILED,
                                 result.ToShortText());
          result.AddDetail(partial->proto.unique_id());
          result.AddDetail(completing->proto.unique_id());
          errors_.push_back(result);
        }
      }
    }

    if (!found_a_pair) {
      errors_.push_back(
          AuditorResult(AuditorResult::Type::ERROR_INCOMPLETED_ANNOTATION,
                        partial->proto.unique_id()));
    }
  }

  for (AnnotationInstance* instance : completing_annotations) {
    if (!base::Contains(used_completing_annotations, instance)) {
      errors_.push_back(
          AuditorResult(AuditorResult::Type::ERROR_INCOMPLETED_ANNOTATION,
                        instance->proto.unique_id()));
    }
  }

  if (new_annotations.size())
    extracted_annotations_.insert(extracted_annotations_.end(),
                                  new_annotations.begin(),
                                  new_annotations.end());
}

void TrafficAnnotationAuditor::AddMissingAnnotations() {
  for (const auto& item : exporter_.GetArchivedAnnotations()) {
    if (item.second.deprecation_date.empty() &&
        exporter_.MatchesCurrentPlatform(item.second) &&
        !item.second.file_path.empty() &&
        !PathFiltersMatch(path_filters_, item.second.file_path)) {
      extracted_annotations_.push_back(AnnotationInstance::LoadFromArchive(
          item.second.type, item.first, item.second.unique_id_hash_code,
          item.second.second_id_hash_code, item.second.content_hash_code,
          item.second.semantics_fields, item.second.policy_fields,
          item.second.file_path));
    }
  }
}

bool TrafficAnnotationAuditor::RunAllChecks(
    bool report_xml_updates) {
  if (exporter_.GetArchivedAnnotations().empty() &&
      !exporter_.LoadAnnotationsXML()) {
    return false;
  }

  std::set<int> deprecated_ids;
  exporter_.GetDeprecatedHashCodes(&deprecated_ids);

  if (!path_filters_.empty())
    AddMissingAnnotations();

  TrafficAnnotationIDChecker id_checker(GetReservedIDsSet(), deprecated_ids);
  id_checker.Load(extracted_annotations_);
  id_checker.CheckIDs(&errors_);

  // Only check annotation contents if IDs are all OK, because if there are
  // id errors, there might be some mismatching annotations and irrelevant
  // content errors.
  if (errors_.empty())
    CheckAnnotationsContents();

  CheckAllRequiredFunctionsAreAnnotated();

  if (errors_.empty()) {
    exporter_.UpdateAnnotations(extracted_annotations_, GetReservedIDsMap(),
                                &errors_);
  }

  // If |report_xml_updates| is true, check annotations.xml whether or not it is
  // modified, as there might be format differences with exporter outputs due to
  // manual updates.
  if (report_xml_updates) {
    std::string updates = exporter_.GetRequiredUpdates();
    if (!updates.empty()) {
      errors_.push_back(AuditorResult(
          AuditorResult::Type::ERROR_ANNOTATIONS_XML_UPDATE, updates));
    }
  }

  return true;
}
