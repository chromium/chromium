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

#include "maldoca/ole/ole_to_proto.h"
#include "maldoca/ole/proto/ole_to_proto_settings.proto.h"

using ::maldoca::ole::OleToProtoSettings;

namespace {
OleToProtoSettings EnableAllSettings() {
  OleToProtoSettings settings;
  settings.set_include_summary_information(true);
  settings.set_include_vba_code(true);
  settings.set_include_directory_structure(true);
  settings.set_include_stream_hashes(true);
  settings.set_include_olenative_metadata(true);
  settings.set_include_olenative_content(true);
  settings.set_include_unknown_strings(true);
  return settings;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  maldoca::OleToProto ole_to_proto(EnableAllSettings());
  security::tag::minimeta::OleFile ole_file;
  ole_to_proto.ParseOleBuffer(
      std::string(reinterpret_cast<const char *>(data), size), &ole_file);
  return 0;
}
