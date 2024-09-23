// Copyright 2014 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/tool_support.h"

#include <stdio.h>

#include <string_view>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "package.h"

namespace crashpad {

// static
void ToolSupport::Version(const base::FilePath& me) {
  fprintf(stderr,
          "%" PRFilePath " (%s) %s\n%s\n",
          me.value().c_str(),
          PACKAGE_NAME,
          PACKAGE_VERSION,
          PACKAGE_COPYRIGHT);
}

// static
void ToolSupport::UsageTail(const base::FilePath& me) {
  fprintf(stderr,
          "\nReport %" PRFilePath " bugs to\n%s\n%s home page: <%s>\n",
          me.value().c_str(),
          PACKAGE_BUGREPORT,
          PACKAGE_NAME,
          PACKAGE_URL);
}

// static
void ToolSupport::UsageHint(const base::FilePath& me, const char* hint) {
  if (hint) {
    fprintf(stderr, "%" PRFilePath ": %s\n", me.value().c_str(), hint);
  }
  fprintf(stderr,
          "Try '%" PRFilePath " --help' for more information.\n",
          me.value().c_str());
}

#if BUILDFLAG(IS_POSIX)
// static
void ToolSupport::Version(const std::string& me) {
  Version(base::FilePath(me));
}

// static
void ToolSupport::UsageTail(const std::string& me) {
  UsageTail(base::FilePath(me));
}

// static
void ToolSupport::UsageHint(const std::string& me, const char* hint) {
  UsageHint(base::FilePath(me), hint);
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)

// static
int ToolSupport::Wmain(int argc, wchar_t* argv[], int (*entry)(int, char* [])) {
  auto argv_as_utf8 = base::HeapArray<char*>::Uninit(argc + 1);
  std::vector<std::string> storage;
  storage.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    storage.push_back(base::WideToUTF8(argv[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argc] = nullptr;
  return entry(argc, argv_as_utf8.data());
}

#endif  // BUILDFLAG(IS_WIN)

// static
base::FilePath::StringType ToolSupport::CommandLineArgumentToFilePathStringType(
    std::string_view path) {
#if BUILDFLAG(IS_POSIX)
  return std::string(path.data(), path.size());
#elif BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(path);
#endif  // BUILDFLAG(IS_POSIX)
}

// static
std::string ToolSupport::FilePathToCommandLineArgument(
    const base::FilePath& file_path) {
#if BUILDFLAG(IS_POSIX)
  return file_path.value();
#elif BUILDFLAG(IS_WIN)
  return base::WideToUTF8(file_path.value());
#endif  // BUILDFLAG(IS_POSIX)
}

}  // namespace crashpad
