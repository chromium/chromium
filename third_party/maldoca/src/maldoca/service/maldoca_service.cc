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

#include "maldoca/service/maldoca_service.h"

#include "maldoca/base/logging.h"
#include "maldoca/service/common/process_doc.h"
#include "maldoca/service/proto/doc_type.pb.h"

namespace maldoca {

::grpc::Status MaldocaServiceImpl::SendProcessingRequest(
    ::grpc::ServerContext* context, const ProcessDocumentRequest* request,
    ProcessDocumentResponse* response) {
  auto status = processor_->ProcessDoc(
      request->file_name(), request->doc_content(), request, response);

  if (!status.ok()) {
    LOG(ERROR) << "Document Processing Failed: " << status;
    *response->add_errors() = utils::TranslateStatusToProto(status);
    return TranslateStatus(status);
  } else {
    DLOG(INFO) << "Finished Document Processing";
    return ::grpc::Status::OK;
  }
}

}  // namespace maldoca
