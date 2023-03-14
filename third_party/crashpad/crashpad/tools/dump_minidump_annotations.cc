// Copyright 2023 The Crashpad Authors
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

#include <getopt.h>

#include "base/files/file_path.h"
#include "client/annotation.h"
#include "util/file/file_reader.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "tools/tool_support.h"

namespace crashpad {
namespace {

void Usage(const base::FilePath& me) {
  // clang-format off
  fprintf(stderr,
"Usage: %" PRFilePath " [OPTION]... PATH\n"
"Dump annotations from minidumps.\n"
"\n"
"      --help                      display this help and exit\n"
"      --version                   output version information and exit\n",
          me.value().c_str());
  // clang-format on
  ToolSupport::UsageTail(me);
}

struct Options {
  const char* minidump;
};

int DumpMinidumpAnnotationsMain(int argc, char* argv[]) {
  const base::FilePath argv0(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  const base::FilePath me(argv0.BaseName());

  enum OptionFlags {
    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionMinidump,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  static constexpr option long_options[] = {
      {"minidump", required_argument, nullptr, kOptionMinidump},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  Options options = {};

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionMinidump: {
        options.minidump = optarg;
        break;
      }
      case kOptionHelp: {
        Usage(me);
        return EXIT_SUCCESS;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
      }
    }
  }
  argc -= optind;
  argv += optind;

  if (!options.minidump) {
    ToolSupport::UsageHint(me, "--minidump is required");
    return EXIT_FAILURE;
  }

  FileReader reader;
  if (!reader.Open(base::FilePath(
      ToolSupport::CommandLineArgumentToFilePathStringType(
          options.minidump)))) {
    return EXIT_FAILURE;
  }

  ProcessSnapshotMinidump snapshot;
  if (!snapshot.Initialize(&reader)) {
    return EXIT_FAILURE;
  }

  for (const ModuleSnapshot* module : snapshot.Modules()) {
    printf("Module: %s\n", module->Name().c_str());
    printf("  Simple Annotations\n");
    for (const auto& kv : module->AnnotationsSimpleMap()) {
      printf("    simple_annotations[\"%s\"] = %s\n",
             kv.first.c_str(), kv.second.c_str());
    }

    printf("  Vectored Annotations\n");
    int index = 0;
    for (const std::string& annotation : module->AnnotationsVector()) {
      printf("    vectored_annotations[%d] = %s\n", index, annotation.c_str());
      index++;
    }

    printf("  Annotation Objects\n");
    for (const AnnotationSnapshot& annotation : module->AnnotationObjects()) {
      printf("    annotation_objects[\"%s\"] = ", annotation.name.c_str());
      if (annotation.type != static_cast<uint16_t>(Annotation::Type::kString)) {

        printf("<non-string value, not printing>\n");
        continue;
      }

      std::string value(reinterpret_cast<const char*>(annotation.value.data()),
                        annotation.value.size());

      printf("%s\n", value.c_str());
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::DumpMinidumpAnnotationsMain(argc, argv);
}
