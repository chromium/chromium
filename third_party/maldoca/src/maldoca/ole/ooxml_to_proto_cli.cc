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

#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "maldoca/base/file.h"
#include "maldoca/ole/ooxml_to_proto.h"
#include "maldoca/service/common/utils.h"

// Displays the proto message outputted for the file `filename`.
// Note that `filename` contains the full path of the file.
void DisplayFileContent(absl::string_view filename) {
  auto status_or_content = maldoca::file::GetContents(filename);

  if (!status_or_content.ok()) {
    DLOG(INFO) << "Unable to open archive!\n";
    return;
  }

  auto status_or_ooxml = maldoca::GetOOXMLFileProto(
      status_or_content.value(),
      maldoca::utils::GetDefaultOoxmlToProtoSettings());

  if (!status_or_content.ok()) {
    DLOG(INFO) << "Unable to parse ooxml!\n";
    return;
  }

  maldoca::ooxml::OOXMLFile ooxml_proto = status_or_ooxml.value();

  ooxml_proto.PrintDebugString();
}

// Set the `file` flag.
ABSL_FLAG(std::string, file, "", "Path of the OOXML file");

// This is for building `ooxml_to_proto` as a Bazel binary.
// Usage: ooxml_to_proto_cli -file=<filename>
// where filename is a path to a ooxml file.
int main(int argc, char **argv) {
  absl::ParseCommandLine(argc, argv);

  std::string file_path = absl::GetFlag(FLAGS_file);

  if (file_path.empty()) {
    std::cerr << "Invalid arguments.\n";
    std::cerr << "Use ooxml_to_proto_cli --help for more info\n";
    exit(1);
  }

  // Displays content of the extracted OOXML file.
  DisplayFileContent(file_path);

  return 0;
}
