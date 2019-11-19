// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_file_filter.h"

#include <fstream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// List of keywords that indicate a file may be related to network traffic
// annotations. This list includes all keywords related to defining annotations
// and all functions that need one.
const char* kRelevantKeywords[] = {
    "network_traffic_annotation",
    "network_traffic_annotation_test_helper",
    "NetworkTrafficAnnotationTag",
    "PartialNetworkTrafficAnnotationTag",
    "DefineNetworkTrafficAnnotation",
    "DefinePartialNetworkTrafficAnnotation",
    "CompleteNetworkTrafficAnnotation",
    "BranchedCompleteNetworkTrafficAnnotation",
    "CreateMutableNetworkTrafficAnnotationTag",
    "NO_TRAFFIC_ANNOTATION_YET",
    "NO_PARTIAL_TRAFFIC_ANNOTATION_YET",
    "MISSING_TRAFFIC_ANNOTATION",
    "TRAFFIC_ANNOTATION_FOR_TESTS",
    "PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS",
    "URLFetcher::Create",  // This one is used with class as it's too generic.
    "CreateRequest",       // URLRequestContext::
    nullptr                // End Marker
};

}  // namespace

TrafficAnnotationFileFilter::TrafficAnnotationFileFilter() = default;

TrafficAnnotationFileFilter::~TrafficAnnotationFileFilter() = default;

void TrafficAnnotationFileFilter::GetFilesFromGit(
    const base::FilePath& source_path) {
  // Change directory to source path to access git and check files.
  base::FilePath original_path;
  base::GetCurrentDirectory(&original_path);
  base::SetCurrentDirectory(source_path);

  std::string git_list;
  if (git_file_for_test_.empty()) {
    const base::CommandLine::CharType* args[] =
#if defined(OS_WIN)
        {FILE_PATH_LITERAL("git.bat"), FILE_PATH_LITERAL("ls-files")};
#else
        {"git", "ls-files"};
#endif
    base::CommandLine cmdline(2, args);

    // Get list of files from git.
    if (!base::GetAppOutput(cmdline, &git_list)) {
      LOG(ERROR) << "Could not get files from git.";
      git_list.clear();
    }
  } else {
    if (!base::ReadFileToString(git_file_for_test_, &git_list)) {
      LOG(ERROR) << "Could not load mock git list file from "
                 << git_file_for_test_.MaybeAsASCII().c_str();
      git_list.clear();
    }
  }

  for (const std::string file_path : base::SplitString(
           git_list, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (IsFileRelevant(file_path))
      git_files_.push_back(file_path);
  }

  base::SetCurrentDirectory(original_path);
}

bool TrafficAnnotationFileFilter::IsFileRelevant(const std::string& file_path) {
  // Check file extension.
  int pos = file_path.length() - 3;

  if (pos < 0 || (strcmp(".mm", file_path.c_str() + pos) &&
                  strcmp(".cc", file_path.c_str() + pos))) {
    return false;
  }

  // Ignore test files to speed up the tests. They would be only tested when
  // filters are disabled.
  pos = file_path.length() - 7;
  if (pos >= 0 && (!strcmp("test.cc", file_path.c_str() + pos) ||
                   !strcmp("test.mm", file_path.c_str() + pos))) {
    return false;
  }

  base::FilePath converted_file_path =
#if defined(OS_WIN)
      base::FilePath(
          base::FilePath::StringPieceType(base::UTF8ToWide(file_path)));
#else
      base::FilePath(base::FilePath::StringPieceType(file_path));
#endif

  // Check file content.
  std::string file_content;
  if (!base::ReadFileToString(converted_file_path, &file_content)) {
    LOG(ERROR) << "Could not open file: " << file_path;
    return false;
  }

  for (int i = 0; kRelevantKeywords[i]; i++) {
    if (file_content.find(kRelevantKeywords[i]) != std::string::npos)
      return true;
  }

  return false;
}

void TrafficAnnotationFileFilter::GetRelevantFiles(
    const base::FilePath& source_path,
    const std::vector<std::string>& ignore_list,
    std::string directory_name,
    std::vector<std::string>* file_paths) {
  if (!git_files_.size())
    GetFilesFromGit(source_path);

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  std::replace(directory_name.begin(), directory_name.end(), L'\\', L'/');
#endif

  size_t name_length = directory_name.length();
  for (const std::string& file_path : git_files_) {
    if (!strncmp(file_path.c_str(), directory_name.c_str(), name_length)) {
      bool ignore = false;
      for (const std::string& ignore_pattern : ignore_list) {
        if (re2::RE2::FullMatch(file_path.c_str(), ignore_pattern)) {
          ignore = true;
          break;
        }
      }
      if (!ignore)
        file_paths->push_back(file_path);
    }
  }
}
