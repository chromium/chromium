/*
 * Copyright 2025 Google LLC
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

#include "fcp/client/attestation/attestation_transparency_verifier.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/confidentialcompute/payload_transparency/payload_transparency.h"
#include "fcp/protos/confidentialcompute/key.pb.h"
#include "fcp/protos/confidentialcompute/payload_transparency.pb.h"
#include "fcp/protos/confidentialcompute/signed_endorsements.pb.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace fcp::client::attestation {
namespace {

using ::fcp::confidential_compute::payload_transparency::VerifySignedPayload;
using ::fcp::confidential_compute::payload_transparency::
    VerifySignedPayloadResult;
using ::fcp::confidentialcompute::Key;
using ::fcp::confidentialcompute::SignedEndorsements;
using ::fcp::confidentialcompute::SignedPayload;

// Returns an error if `actual_claims` is not a superset of `expected_claims`.
absl::Status CheckRequiredClaims(
    const google::protobuf::RepeatedPtrField<std::string>& actual_claims,
    const std::vector<std::string>& expected_claims) {
  std::vector<absl::string_view> missing_claims;
  for (const auto& expected : expected_claims) {
    // The number of claims is expected to be small enough that repeated linear
    // searches are fast enough.
    if (absl::c_find(actual_claims, expected) == actual_claims.end()) {
      missing_claims.push_back(expected);
    }
  }
  return missing_claims.empty() ? absl::OkStatus()
                                : absl::InvalidArgumentError(absl::StrCat(
                                      "missing required claims: ",
                                      absl::StrJoin(missing_claims, ", ")));
}

}  // namespace

absl::StatusOr<AttestationTransparencyVerifier::VerificationResult>
AttestationTransparencyVerifier::Verify(const SignedPayload& pipeline_config,
                                        const SignedPayload& encryption_key) {
  const absl::Time now = absl::Now();

  // Verify that the encryption key is properly signed, included in a
  // transparency log (if required by `transparency_log_options`), and valid.
  ABSL_ASSIGN_OR_RETURN(
      VerifySignedPayloadResult encryption_key_result,
      VerifySignedPayload(encryption_key, options_.kms_verifying_keys(),
                          options_.transparency_log_options(), now));
  Key key;
  if (!key.ParseFromString(encryption_key.payload())) {
    return absl::InvalidArgumentError("failed to parse encryption key");
  }
  if (encryption_key_result.headers.empty()) {
    return absl::InternalError("VerifySignedPayload returned no headers");
  }
  // The access policy hash should be in the encryption key's signature's
  // headers.
  absl::string_view access_policy_sha256 =
      encryption_key_result.headers.back().access_policy_sha256();
  if (access_policy_sha256.empty()) {
    return absl::InvalidArgumentError(
        "encryption key headers missing access policy SHA-256");
  }
  // The Oak application signature should be in the most deeply nested
  // Signature.verifying_key's headers, which will be the first entry in
  // `encryption_key_result.headers` per the VerifySignedPayload documentation.
  SignedPayload::Signature::Headers oak_application_signature_headers;
  if (!oak_application_signature_headers.ParseFromString(
          encryption_key_result.headers.front()
              .oak_application_signature()
              .headers())) {
    return absl::InvalidArgumentError(
        "failed to parse oak application signature headers");
  }
  if (oak_application_signature_headers.endorsed_evidence_sha256().empty()) {
    return absl::InvalidArgumentError(
        "oak application signature headers missing endorsed evidence SHA-256");
  }
  ABSL_RETURN_IF_ERROR(CheckRequiredClaims(
      encryption_key_result.headers.front().claims(),
      {
          // Built from open source.
          "https://github.com/project-oak/oak/blob/main/docs/tr/claim/92939.md",
      }));

  // Verify that the pipeline configuration is properly signed, included in a
  // transparency log, and valid.
  ABSL_ASSIGN_OR_RETURN(
      VerifySignedPayloadResult pipeline_config_result,
      VerifySignedPayload(pipeline_config,
                          options_.access_policy_verifying_keys(),
                          options_.transparency_log_options(), now));
  SignedEndorsements::PipelineConfiguration config;
  if (!config.ParseFromString(pipeline_config.payload())) {
    return absl::InvalidArgumentError("failed to parse pipeline configuration");
  }
  // The pipeline configuration and the upload encryption key both contain the
  // SHA-256 digest of the access policy, so the full access policy doesn't need
  // to be downloaded: the same level of transparency is maintained while
  // reducing download sizes. The full policy can be retrieved using the digest
  // (see proto field comments).
  if (config.access_policy_sha256() != access_policy_sha256) {
    return absl::InvalidArgumentError(
        "pipeline configuration access policy SHA-256 does not match "
        "encryption key");
  }

  std::string key_id = key.key_id();
  return VerificationResult{
      .public_key = std::move(key),
      .key_id = std::move(key_id),
      .access_policy_sha256 = std::move(*config.mutable_access_policy_sha256()),
  };
}

}  // namespace fcp::client::attestation
