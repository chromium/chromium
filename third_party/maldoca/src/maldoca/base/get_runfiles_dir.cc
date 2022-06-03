// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/base/get_runfiles_dir.h"

#include "absl/status/status.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status_macros.h"

// NOTE: Adopted from tensorflow/core/platform/env.cc.
#if defined(__APPLE__)
// TODO(#110) Mac-specific headers.
#error "__APPLE__ not supported."
#endif

#if defined(__FreeBSD__)
// TODO(#110) FreeBSD-specific headers.
#error "__FreeBSD__ not supported."
#endif

#if defined(_WIN32)
#include <windows.h>
#define PATH_MAX MAX_PATH
#include "base/strings/utf_string_conversions.h"
#else
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace maldoca {
namespace {

// NOTE: Adopted from tensorflow/core/platform/env.cc.
// TODO(#110) Add more platform-specific code.
std::string GetExecutablePath() {
  char exe_path[PATH_MAX] = {0};
#ifdef __APPLE__
#error "__APPLE__ not supported.";
#elif defined(__FreeBSD__)
#error "__FreeBSD__ not supported.";
#elif defined(_WIN32)
  HMODULE hModule = GetModuleHandleW(NULL);
  WCHAR wc_file_path[MAX_PATH] = {0};
  GetModuleFileNameW(hModule, wc_file_path, MAX_PATH);
  // see
  // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=cmd
  std::string file_path("\\\\?\\");
  file_path.append(base::WideToUTF8(wc_file_path));
  std::copy(file_path.begin(), file_path.end(), exe_path);
#else
  char buf[PATH_MAX] = {0};
  int path_length = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  CHECK_NE(-1, path_length);

  if (strstr(buf, "python") != nullptr) {
    // Discard the path of the python binary, and any flags.
    int fd = open("/proc/self/cmdline", O_RDONLY);
    int cmd_length = read(fd, buf, PATH_MAX - 1);
    CHECK_NE(-1, cmd_length);
    int token_pos = 0;
    for (bool token_is_first_or_flag = true; token_is_first_or_flag;) {
      // Get token length, including null
      int token_len = strlen(&buf[token_pos]) + 1;
      token_is_first_or_flag = false;
      // Check if we can skip without overshooting
      if (token_pos + token_len < cmd_length) {
        token_pos += token_len;
        token_is_first_or_flag = (buf[token_pos] == '-');  // token is a flag
      }
    }
    snprintf(exe_path, sizeof(exe_path), "%s", &buf[token_pos]);
  } else {
    snprintf(exe_path, sizeof(exe_path), "%s", buf);
  }

#endif
  // Make sure it's null-terminated:
  exe_path[sizeof(exe_path) - 1] = 0;

  return exe_path;
}

absl::Status IsDirectory(const std::string& name) {
#ifdef __APPLE__
#error "__APPLE__ not supported.";
#elif defined(__FreeBSD__)
#error "__FreeBSD__ not supported.";
#else
  struct stat sbuf;
  if (stat(name.c_str(), &sbuf) != 0) {
    return absl::InternalError(
        absl::StrCat("stat failed with error: ", strerror(errno)));
  }
#if defined(_WIN32)
  if (!(sbuf.st_mode & S_IFDIR)) {
#else
  if (!S_ISDIR(sbuf.st_mode)) {
#endif  // _WIN32
    return absl::FailedPreconditionError("Not a directory.");
  }

  return absl::OkStatus();
#endif
}

}  // namespace

std::string GetRunfilesDir() {
  std::string bin_path = GetExecutablePath();
  std::string runfiles_suffix = ".runfiles/com_google_maldoca";
  std::size_t pos = bin_path.find(runfiles_suffix);

  // Sometimes (when executing under python) bin_path returns the full path to
  // the python scripts under runfiles. Get the substring.
  if (pos != std::string::npos) {
    return bin_path.substr(0, pos + runfiles_suffix.length());
  }

  // See if we have the executable path. if executable.runfiles exists, return
  // that folder.
  std::string runfiles_path = bin_path + runfiles_suffix;
  absl::Status s = IsDirectory(runfiles_path);
  if (s.ok()) {
    return runfiles_path;
  }

  // If nothing can be found, return something close.
  return bin_path.substr(0, bin_path.find_last_of("/\\"));
}

}  // namespace maldoca
