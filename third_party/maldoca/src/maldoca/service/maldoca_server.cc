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

#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/text_format.h"
#include "maldoca/base/logging.h"
#include "maldoca/service/common/utils.h"
#include "maldoca/service/maldoca_service.h"
#include "maldoca/service/proto/processing_config.pb.h"

using grpc::Server;
using grpc::ServerBuilder;

ABSL_FLAG(uint16_t, port, 6666, "Port for communication");

#ifndef MALDOCA_CHROME
ABSL_FLAG(std::string, log_dir, "/tmp", "Base of the log directory");
ABSL_FLAG(std::string, log_file_name, "maldoca_server", "Base log name");
ABSL_FLAG(int, vlog_level, 0,
          "VLOG with level equal to or below this level is logged");
ABSL_FLAG(std::string, process_config_text_proto, R"(
  handler_configs {
      key: "office_parser"
      value {
        doc_type: OFFICE
        parser_config { handler_type: DEFAULT_OFFICE_PARSER use_sandbox: false }
      }
    }
    handler_configs {
      key: "pdf_parser"
      value {
        doc_type: PDF
        parser_config { handler_type: DEFAULT_PDF_PARSER use_sandbox: false }
      }
    }
    handler_configs {
      key: "office_extractor"
      value {
        doc_type: OFFICE
        feature_extractor_config {
          handler_type: DEFAULT_OFFICE_FEATURE_EXTRACTOR
        }
        dependencies: "office_parser"
      }
    }
    handler_configs {
      key: "pdf_extractor"
      value {
        doc_type: PDF
        feature_extractor_config { handler_type: DEFAULT_PDF_FEATURE_EXTRACTOR }
        dependencies: "pdf_parser"
      }
    }
    handler_configs {
      key: "pdf_export"
      value {
        doc_type: PDF
        feature_export_config { handler_type: DEFAULT_PDF_EXPORT }
        dependencies: "pdf_extractor"
      }
    }
)",
          "ProcessorConfig proto as text to config the processing pipeline");
#else
ABSL_FLAG(std::string, process_config_text_proto, R"(
  handler_configs {
      key: "office_parser"
      value {
        doc_type: OFFICE
        parser_config { handler_type: DEFAULT_OFFICE_PARSER use_sandbox: false }
      }
    }
    handler_configs {
      key: "office_extractor"
      value {
        doc_type: OFFICE
        feature_extractor_config {
          handler_type: DEFAULT_OFFICE_FEATURE_EXTRACTOR
        }
        dependencies: "office_parser"
      }
    }
)",
          "ProcessorConfig proto as text to config the processing pipeline");
#endif
// MALDOCA_CHROME

constexpr uint32_t kMaxSendReceiveMessageSize = 64 * 1024 * 1024;

static void Run() {
  std::string address(absl::StrCat("0.0.0.0:", absl::GetFlag(FLAGS_port)));
  ::maldoca::ProcessorConfig config;
  CHECK(google::protobuf::TextFormat::ParseFromString(
      absl::GetFlag(FLAGS_process_config_text_proto), &config));
  ::maldoca::DocProcessor processor(config);
  ::maldoca::MaldocaServiceImpl service(&processor);

  ServerBuilder builder;
  // TODO(X): fix credentials
  builder.SetMaxReceiveMessageSize(kMaxSendReceiveMessageSize);
  builder.SetMaxSendMessageSize(kMaxSendReceiveMessageSize);
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  LOG(INFO) << "Server listening on port: " << address;

  server->Wait();
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
#ifdef MALDOCA_CHROME
  ::maldoca::InitLogging();
#else
  auto log_dir = absl::GetFlag(FLAGS_log_dir);
  auto log_file_name = absl::GetFlag(FLAGS_log_file_name);
  // Only init log if the file location is set. Otherwise let it go to the
  // default location.
  if (!log_file_name.empty() && !log_dir.empty()) {
    ::maldoca::InitLogging(log_dir.c_str(), log_file_name.c_str(),
                           absl::GetFlag(FLAGS_vlog_level));
  }
#endif  // MALDOCA_CHROME
  Run();
  return 0;
}
