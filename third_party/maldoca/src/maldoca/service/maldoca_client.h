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

// Client for Maldoca server
#ifndef MALDOCA_SERVICE_MALDOCA_CLIENT_H_
#define MALDOCA_SERVICE_MALDOCA_CLIENT_H_

// #include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <memory>
#include <string>

#include "maldoca/service/proto/maldoca_service.grpc.pb.h"

namespace maldoca {
class MaldocaClient {
 public:
  explicit MaldocaClient(std::shared_ptr<::grpc::Channel> channel)
      : stub_(Maldoca::NewStub(channel)) {}

  ::grpc::Status ProcessDocument(const ProcessDocumentRequest& request,
                                 ProcessDocumentResponse* response);

 private:
  std::unique_ptr<Maldoca::Stub> stub_;
};
}  // namespace maldoca
#endif
