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

#ifndef FCP_CLIENT_ATTESTATION_ATTESTATION_TRANSPARENCY_VERIFIER_H_
#define FCP_CLIENT_ATTESTATION_ATTESTATION_TRANSPARENCY_VERIFIER_H_

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/confidentialcompute/payload_transparency.pb.h"

namespace fcp::client::attestation {

// An AttestationTransparencyVerifier requires that the upload encryption key
// and pipeline configuration are signed by authorized verifying keys and are
// (optionally) included in a transparency log such as Rekor.
//
// This class does not verify TEE evidence and endorsements itself, but it
// ensures that there is sufficient information to perform that verification in
// AttestationVerificationRecord and (optionally) in a transparency log.
class AttestationTransparencyVerifier : public AttestationVerifier {
 public:
  // Constructs a new AttestationTransparencyVerifier.
  explicit AttestationTransparencyVerifier(
      confidentialcompute::AccessPolicyEndorsementOptions options)
      : options_(std::move(options)) {}

  absl::StatusOr<VerificationResult> Verify(
      const confidentialcompute::SignedPayload& pipeline_config,
      const confidentialcompute::SignedPayload& encryption_key) override;

 private:
  confidentialcompute::AccessPolicyEndorsementOptions options_;
};

}  // namespace fcp::client::attestation

#endif  // FCP_CLIENT_ATTESTATION_ATTESTATION_TRANSPARENCY_VERIFIER_H_
