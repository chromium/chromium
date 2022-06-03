/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Classes to serve Maldoca server requests

#ifndef MALDOCA_SERVICE_MALDOCA_SERVICE_H_
#define MALDOCA_SERVICE_MALDOCA_SERVICE_H_

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

#include <memory>

#include "maldoca/base/status_macros.h"
#include "maldoca/service/common/process_doc.h"
#include "maldoca/service/common/utils.h"
#include "maldoca/service/proto/maldoca_service.grpc.pb.h"

namespace maldoca {

inline ::grpc::Status TranslateStatus(const absl::Status& status) {
  return ::grpc::Status(static_cast<::grpc::StatusCode>(status.code()),
                        std::string(status.message()));
}

class MaldocaServiceImpl final : public Maldoca::Service {
 public:
  explicit MaldocaServiceImpl(DocProcessor* processor) : processor_(processor) {
    CHECK(processor != nullptr);
  }

 protected:
  ::grpc::Status SendProcessingRequest(
      ::grpc::ServerContext* context, const ProcessDocumentRequest* request,
      ProcessDocumentResponse* response) override;

 private:
  DocProcessor* processor_;
};
}  // namespace maldoca
#endif
