// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// For expedience, we are borrowing file ops in the protobuf/testing.
// In the long run this should be replaced by either using better
// libraries or rewrite.

#include "maldoca/base/file.h"

#ifndef MALDOCA_CHROME
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // MALDOCA_CHROME

#include <cerrno>
#include <cstdlib>
#include <ctime>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/logging.h"

#ifndef MALDOCA_CHROME
#include "google/protobuf/text_format.h"  // nogncheck
#include "maldoca/base/ret_check.h"
#include "maldoca/base/status_macros.h"
#include "re2/re2.h"
#endif  // MALDOCA_CHROME

// Key to xor-decode testdata files as a safety measure and to e.g. avoid being
// deleted by AVs.
constexpr uint8_t kXorKey = 0x42;

using absl::Status;

#ifndef MALDOCA_CHROME
using ::google::protobuf::Message;
#endif

namespace maldoca {
namespace file {
namespace {

// Wrapper for making a FILE* using fopen so that it'll close on exit scope. Do
// not call fclose on the included FILE*
std::unique_ptr<FILE, void (*)(FILE*)> FileCloser(const std::string& path,
                                                  const char* mode) {
  return {std::fopen(path.c_str(), mode), +[](FILE* fp) {
            if (fp) fclose(fp);
          }};
}

#ifndef MALDOCA_CHROME
// TODO(somebody): Fix this to work with windows
static LazyRE2 kWildcardRE = {"\\?|\\[|\\]|\\*"};
constexpr absl::string_view kPathSeparator = "/";

bool IsAbsolutePath(absl::string_view path) {
  return absl::StartsWith(path, kPathSeparator);
}

// Returns true if a path wildcard (*?[]) characters is found in path.
bool HasWildCard(absl::string_view path) {
  return RE2::PartialMatch({path.data(), path.size()}, *kWildcardRE);
}

// Change the file pattern wildcard into regex (* -> .*, ? -> .,  . -> \.)
// that match the same wildcard pattern
std::unique_ptr<RE2> PathToRe2(absl::string_view path) {
  auto replaced =
      absl::StrReplaceAll(path, {{"*", ".*"}, {"?", "."}, {".", "\\."}});
  return absl::make_unique<RE2>(absl::StrCat("^", replaced, "$"));
}

// Find the leading suppath of pattern that doesn't contain any wildcard.
// Return the found path as a member of prefiexes.  For the rest of the path,
// create REs that match each part so the path.
// E.g. if patern is /home/me/whatever*/file??.txt; prefixes gets '/home/me'
// and the res gets RE("^whatever.*$"), RE("^file..\.txt$"). If the pattern
// is a relative path and there is no non wildcard prefix, e.g. "file*.txt"
// then res gets ".".
Status FindNonWildPrefix(absl::string_view pattern, std::string* prefix,
                         std::vector<std::unique_ptr<RE2>>* res) {
  if (pattern.empty()) {
    return absl::InvalidArgumentError("Input pattern is empty");
  }
  bool is_absolute = IsAbsolutePath(pattern);
  std::string pre = is_absolute ? "" : ".";

  bool found_wild = false;
  for (auto part : absl::StrSplit(pattern, kPathSeparator, absl::SkipEmpty())) {
    if (found_wild) {
      // already seen a wildcard, continue to create REs
      res->push_back(PathToRe2(part));
    } else if (!HasWildCard(part)) {
      absl::StrAppend(&pre, kPathSeparator, part);
    } else {
      // has wildcard, start creating REs
      found_wild = true;
      res->push_back(PathToRe2(part));
    }
  }
  *prefix = std::move(pre);
  return absl::OkStatus();
}

// Find all entries under thw in_path whose name matches re and return the
// pathes of found entries in out_path.  If match_dir is true, only match
// directory entries. It always ignore entries . and ..
Status NextMatchingPaths(const RE2& re, bool match_dir,
                         const std::string& in_path,
                         std::vector<std::string>* out_path) {
  DIR* dp = nullptr;
  // iterate through entries in in_p
  dp = opendir(in_path.c_str());
  if (dp != nullptr) {
    for (dirent* entry = readdir(dp); entry != nullptr; entry = readdir(dp)) {
      // skip '.' and '..'
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }
      std::string curr_path = JoinPath(in_path, entry->d_name);
      struct stat buff;
      stat(curr_path.c_str(), &buff);
      bool is_dir = buff.st_mode & S_IFDIR;
      // if only looking for dir and is not a dir, skip.
      if (match_dir && !is_dir) {
        continue;
      }
      // push_back found elements
      if (RE2::FullMatch(entry->d_name, re)) {
        out_path->push_back(std::move(curr_path));
      }
    }
  }
  return absl::OkStatus();
}

// Find mahcing paths for every element of in_path
Status NextMatchingPaths(const RE2& re, bool match_dir,
                         const std::vector<std::string>& in_path,
                         std::vector<std::string>* out_path) {
  for (const auto& p : in_path) {
    MALDOCA_RETURN_IF_ERROR(NextMatchingPaths(re, match_dir, p, out_path));
  }
  return absl::OkStatus();
}
#endif  // MALDOCA_CHROME

std::pair<absl::string_view, absl::string_view> SplitPathOn(
    absl::string_view path, absl::string_view separator) {
  auto pos = path.find_last_of(separator);

  // Handle the case with no '/' in 'path'.
  if (pos == absl::string_view::npos)
    return std::make_pair(path.substr(0, 0), path);

  // Handle the case with a single leading '/' in 'path'.
  if (pos == 0)
    return std::make_pair(
        path.substr(0, 1),
        path.size() == 1 ? path.substr(0, 0) : path.substr(1, path.size() - 1));

  absl::string_view filename;
  auto pos_1 = pos + 1;
  if (path.size() > pos_1) {
    filename = path.substr(pos_1, path.size() - pos_1);
  }
  return std::make_pair(path.substr(0, pos), filename);
}
}  // namespace

#ifndef MALDOCA_CHROME
absl::Status Match(absl::string_view pattern,
                   std::vector<std::string>* filenames) {
  std::vector<std::string> prefixes;
  std::vector<std::unique_ptr<RE2>> subdir_res;
  std::string prefix;
  MALDOCA_RETURN_IF_ERROR(
      FindNonWildPrefix(pattern, &prefix, &subdir_res));  // The prefix path
  prefixes.push_back(std::move(prefix));
  for (int i = 0; i < subdir_res.size(); ++i) {
    std::vector<std::string> old_prefixes(std::move(prefixes));
    MALDOCA_RETURN_IF_ERROR(NextMatchingPaths(
        *subdir_res[i], (i != subdir_res.size() - 1), old_prefixes, &prefixes));
    if (prefixes.empty()) {
      return absl::NotFoundError(
          absl::StrCat("No file matching ", pattern, " found"));
    }
  }
  *filenames = std::move(prefixes);
  return absl::OkStatus();
}
#endif  // MALDOCA_CHROME

// TODO(someone): Make this work with general file systems.
absl::Status GetContents(const std::string& path, std::string* contents,
                         bool xor_decode_file) {
  auto fc = FileCloser(path, "rb");
  auto fp = fc.get();
  if (fp == nullptr) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Can't open file ", path, " with error: ", strerror(errno)));
  }
  contents->clear();
  char buf[4096];
  while (!feof(fp)) {
    size_t ret = fread(buf, 1, 4096, fp);
    auto error = ferror(fp);
    if (ret == 0 && error) {
      return absl::InternalError(absl::StrCat(
          "Failed reading ", path, " with error: ", strerror(errno)));
    }
    absl::StrAppend(contents, absl::string_view(buf, ret));
  }
  // xor decode the file's content if requested.
  if (xor_decode_file) {
    DLOG(INFO) << "xor decoding file: " << path;
    for (int i = 0; i < contents->size(); i++) {
      (*contents)[i] = (*contents)[i] ^ kXorKey;
    }
  }
  return absl::OkStatus();
}

std::pair<absl::string_view, absl::string_view> SplitFilename(
    absl::string_view path) {
  auto base_ext = SplitPathOn(path, ".");
  // check if no "." is found then swap the base ext due to the implemetation
  // that puts data in the first half when no split happens.
  if (base_ext.first.size() + base_ext.second.size() == path.size()) {
    return {base_ext.second, base_ext.first};
  }
  return base_ext;
}

StatusOr<std::string> GetContents(absl::string_view path,
                                  bool xor_decode_file) {
  std::string output;
  auto status = GetContents(std::string(path), &output, xor_decode_file);
  if (status.ok()) {
    return output;
  } else {
    return status;
  }
}

#ifndef MALDOCA_CHROME
absl::Status SetContents(const std::string& path, absl::string_view contents) {
  // "wb" is safer for windows than "w" as it may replace LF with CRLF in "w".
  auto fc = FileCloser(path, "wb");
  auto fp = fc.get();
  if (fp == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Can't open file ", path));
  }

  fwrite(contents.data(), sizeof(char), contents.size(), fp);
  auto error = ferror(fp);
  if (error) {
    return absl::InternalError(absl::StrCat("Failed writing ", path,
                                            " with error: ", strerror(errno)));
  }
  return absl::OkStatus();
}

absl::Status CreateDir(const std::string& path, int mode) {
  if (path.empty()) {
    return absl::InvalidArgumentError("path is empty.");
  }
  std::error_code error_code;
  if (mkdir(path.c_str(), mode) == 0) {
    return absl::OkStatus();
  } else {
    return absl::InternalError(absl::StrCat("Failed creating dir ", path,
                                            " with error: ", strerror(errno)));
  }
}

std::string CreateTempFileAndCloseOrDie(absl::string_view directory,
                                        const std::string& contents) {
  int64_t now_us = absl::ToUnixMicros(absl::Now());
  uint32_t pid = static_cast<uint32_t>(getpid());
  std::string file_name =
      JoinPath(directory, absl::StrCat("temp_", pid, "-", now_us));
  CHECK(SetContents(file_name, contents).ok());
  return file_name;
}

absl::Status GetTextProto(absl::string_view filename, Message* proto) {
  auto status_or = file::GetContents(filename);
  if (!status_or.ok()) return status_or.status();
  if (google::protobuf::TextFormat::ParseFromString(status_or.value(), proto)) {
    return absl::OkStatus();
  } else {
    // TODO(somebody): return better status
    return absl::Status(
        absl::StatusCode::kUnknown,
        absl::StrCat("Failed parse proto from file ", filename));
  }
}
#endif  // MALDOCA_CHROME
}  // namespace file
}  // namespace maldoca
