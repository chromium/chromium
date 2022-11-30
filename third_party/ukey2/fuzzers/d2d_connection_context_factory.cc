// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/ukey2/fuzzers/d2d_connection_context_factory.h"

#include <string>

#include "base/check.h"
#include "third_party/ukey2/src/src/main/cpp/include/securegcm/ukey2_handshake.h"

namespace securegcm {

namespace {

const securegcm::UKey2Handshake::HandshakeCipher kCipher =
    securegcm::UKey2Handshake::HandshakeCipher::P256_SHA512;
// Arbitrary chosen length as verification string is discarded regardless.
const int32_t kMaxUkey2VerificationStringLength = 32;

void PerformHandshake(UKey2Handshake* server, UKey2Handshake* client) {
  std::unique_ptr<std::string> client_init = client->GetNextHandshakeMessage();
  CHECK(client_init) << client->GetLastError();

  UKey2Handshake::ParseResult parse_result =
      server->ParseHandshakeMessage(*client_init);
  CHECK(parse_result.success) << server->GetLastError();

  std::unique_ptr<std::string> server_init = server->GetNextHandshakeMessage();
  CHECK(server_init) << server->GetLastError();

  client->ParseHandshakeMessage(*server_init);
  CHECK(parse_result.success) << client->GetLastError();

  std::unique_ptr<std::string> client_finish =
      client->GetNextHandshakeMessage();
  CHECK(client_finish) << client->GetLastError();

  parse_result = server->ParseHandshakeMessage(*client_finish);
  CHECK(parse_result.success) << server->GetLastError();
}

}  // namespace

std::unique_ptr<D2DConnectionContextV1> CreateServerContext() {
  std::unique_ptr<UKey2Handshake> server =
      UKey2Handshake::ForResponder(kCipher);
  CHECK(server);

  std::unique_ptr<UKey2Handshake> client =
      UKey2Handshake::ForInitiator(kCipher);
  CHECK(client);

  PerformHandshake(server.get(), client.get());

  std::unique_ptr<std::string> verification_string =
      server->GetVerificationString(kMaxUkey2VerificationStringLength);
  CHECK(verification_string) << server->GetLastError();

  bool verify_result = server->VerifyHandshake();
  CHECK(verify_result) << server->GetLastError();

  return server->ToConnectionContext();
}

std::unique_ptr<D2DConnectionContextV1> CreateClientContext() {
  auto server = UKey2Handshake::ForResponder(kCipher);
  CHECK(server);

  auto client = UKey2Handshake::ForInitiator(kCipher);
  CHECK(client);

  PerformHandshake(server.get(), client.get());

  std::unique_ptr<std::string> verification_string =
      client->GetVerificationString(kMaxUkey2VerificationStringLength);
  CHECK(verification_string) << client->GetLastError();

  bool verify_result = client->VerifyHandshake();
  CHECK(verify_result) << client->GetLastError();

  return client->ToConnectionContext();
}

}  // namespace securegcm
