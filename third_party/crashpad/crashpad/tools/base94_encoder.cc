// Copyright 2019 The Crashpad Authors
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
#include <stdio.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "tools/tool_support.h"
#include "util/stream/file_encoder.h"

namespace crashpad {
namespace {

void Usage(const base::FilePath& me) {
  // clang-format off
  fprintf(stderr,
"Usage: %" PRFilePath " [options] <input-file> <output-file>\n"
"Encode/Decode the given file\n"
"\n"
"  -e, --encode   compress and encode the input file to a base94 encoded"
                  " file\n"
"  -d, --decode   decode and decompress a base94 encoded file\n"
"      --help     display this help and exit\n"
"      --version  output version information and exit\n",
          me.value().c_str());
  // clang-format on
  ToolSupport::UsageTail(me);
}

int Base94EncoderMain(int argc, char* argv[]) {
  const base::FilePath argv0(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  const base::FilePath me(argv0.BaseName());

  enum OptionFlags {
    // “Short” (single-character) options.
    kOptionEncode = 'e',
    kOptionDecode = 'd',

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  struct Options {
    bool encoding;
    base::FilePath input_file;
    base::FilePath output_file;
  } options = {};

  static constexpr option long_options[] = {
      {"encode", no_argument, nullptr, kOptionEncode},
      {"decode", no_argument, nullptr, kOptionDecode},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  bool encoding_valid = false;
  int opt;
  while ((opt = getopt_long(argc, argv, "de", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionEncode:
        options.encoding = true;
        encoding_valid = true;
        break;
      case kOptionDecode:
        options.encoding = false;
        encoding_valid = true;
        break;
      case kOptionHelp:
        Usage(me);
        return EXIT_SUCCESS;
      case kOptionVersion:
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      default:
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
    }
  }

  if (!encoding_valid) {
    ToolSupport::UsageHint(me, "Either -e or -d required");
    return EXIT_FAILURE;
  }

  argc -= optind;
  argv += optind;
  if (argc != 2) {
    ToolSupport::UsageHint(me, "Both input-file and output-file required");
    return EXIT_FAILURE;
  }

  options.input_file = base::FilePath(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  options.output_file = base::FilePath(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[1]));

  FileEncoder encoder(options.encoding ? crashpad::FileEncoder::Mode::kEncode
                                       : crashpad::FileEncoder::Mode::kDecode,
                      options.input_file,
                      options.output_file);
  return encoder.Process() ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace
}  // namespace crashpad

#if BUILDFLAG(IS_POSIX)
int main(int argc, char* argv[]) {
  return crashpad::Base94EncoderMain(argc, argv);
}
#elif BUILDFLAG(IS_WIN)
int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::Base94EncoderMain);
}
#endif  // BUILDFLAG(IS_POSIX)
