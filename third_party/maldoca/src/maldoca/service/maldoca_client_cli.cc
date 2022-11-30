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

// An example of using maldoca client. It sends the --input_file to the server
// and get the processed result and save in --output_file
// Example:  First build maldoca/service/maldoca_server then run
// <path>/maldoca_server  # --logtostderr
// Build client
// <path>/maldocal_client_example --output_format=text --input_file='...'
// then the output is dumped as text to stdout

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "maldoca/base/file.h"
#include "maldoca/base/logging.h"
#include "maldoca/service/maldoca_client.h"

ABSL_FLAG(uint16_t, port, 6666, "port for communication");
ABSL_FLAG(std::string, host, "localhost", "Host address");
ABSL_FLAG(std::string, input_file, "", "input file");
#ifndef MALDOCA_CHROME
ABSL_FLAG(std::string, output_file, "", "output file, if empty goes to stdout");
#endif  // MALDOCA_CHROME
ABSL_FLAG(std::string, output_format, "raw",
          "output format, one of raw|json|text");

static std::string ConvertParserOutput(
    const ::maldoca::ProcessDocumentResponse& resp,
    const std::string& output_format) {
  std::string output;
  if (output_format == "raw") {
    CHECK(resp.SerializeToString(&output));
  } else if (output_format == "json") {
    CHECK(::google::protobuf::util::MessageToJsonString(resp, &output).ok());
  } else {  // We validate the flag in main so this is "text"
    output = resp.DebugString();
  }
  return output;
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  std::string output_format = absl::GetFlag(FLAGS_output_format);
  CHECK(output_format == "raw" || output_format == "text" ||
        output_format == "json")
      << "Invalid output type:" << output_format;
  std::string input_file = absl::GetFlag(FLAGS_input_file);
  CHECK(!input_file.empty());
#ifndef MALDOCA_CHROME
  std::string output_file = absl::GetFlag(FLAGS_output_file);
#endif  // MALDOCA_CHROME
  std::string host = absl::GetFlag(FLAGS_host);
  CHECK(!host.empty());
  std::string address = absl::StrCat(host, ":", absl::GetFlag(FLAGS_port));
  ::maldoca::MaldocaClient client(
      ::grpc::CreateChannel(address, ::grpc::InsecureChannelCredentials()));
  ::maldoca::ProcessDocumentRequest req;
  req.set_file_name(input_file);
  ::maldoca::ProcessDocumentResponse resp;
  CHECK(
      ::maldoca::file::GetContents(input_file, req.mutable_doc_content()).ok());
  auto rpc_status = client.ProcessDocument(req, &resp);
  if (!rpc_status.ok()) {
    std::cerr << "**Failed** " << rpc_status.error_message() << "\n"
              << resp.DebugString();
    std::cerr << "\n**Failed** " << rpc_status.error_message() << "\n";
    exit(1);
  }
  auto output = ConvertParserOutput(resp, output_format);
#ifndef MALDOCA_CHROME
  if (output_file.empty()) {
    // write to stdout
#endif  // MALDOCA_CHROME
    std::cout << output;
#ifndef MALDOCA_CHROME
  } else {
    CHECK(::maldoca::file::SetContents(output_file, output).ok());
  }
#endif  // MALDOCA_CHROME
  return 0;
}
