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

#include <stdlib.h>

#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"
#include "maldoca/base/file.h"
#include "maldoca/ole/ole_to_proto.h"

using ::maldoca::ole::OleToProtoSettings;

int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  if (argc != 2) {
    absl::FPrintF(stderr, "Usage: ole_to_proto_cli <filename>\n");
    exit(1);
  }

  auto status_or_contents = maldoca::file::GetContents(argv[1]);
  if (!status_or_contents.ok()) {
    absl::FPrintF(stderr, "While reading %s: %s\n", argv[1],
                  status_or_contents.status().ToString());
    exit(1);
  }
  const auto &contents = status_or_contents.value();

  OleToProtoSettings settings;
  settings.set_include_olenative_content(true);
  settings.set_include_unknown_strings(true);
  auto status_or_proto = maldoca::GetOleFileProto(contents, settings);

  absl::PrintF("%s\n", status_or_proto.value().DebugString());

  return 0;
}
