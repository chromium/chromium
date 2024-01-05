// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "third_party/protobuf/src/google/protobuf/stubs/logging.h"
#include "third_party/ukey2/fuzzers/d2d_connection_context_factory.h"

// Disable noisy logging in protobuf.
google::protobuf::LogSilencer log_silencer;

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    // Disable noisy logging in securemessage.
    std::cerr.setstate(std::ios_base::failbit);

    // Create instance once to be reused between fuzzing rounds.
    client_context = securegcm::CreateClientContext();
    CHECK(client_context);
  }

  std::unique_ptr<securegcm::D2DConnectionContextV1> client_context;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::NoDestructor<Environment> environment;

  std::string buffer(data, data + size);
  environment->client_context->DecodeMessageFromPeer(buffer);

  return 0;
}
