#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
shopt -s extglob dotglob
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

FILES=(
  "proto/attestation/assertion.proto"
  "proto/attestation/endorsement.proto"
  "proto/attestation/eventlog.proto"
  "proto/attestation/evidence.proto"
  "proto/attestation/reference_value.proto"
  "proto/attestation/tcb_version.proto"
  "proto/crypto/certificate.proto"
  "proto/crypto/crypto.proto"
  "proto/digest.proto"
  "proto/session/messages.proto"
  "proto/session/session.proto"
  "proto/validity.proto"
  "proto/variant.proto"
)

for f in "${FILES[@]}"; do
  cp --update=all "${SCRIPT_DIR}/src/${f}" "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google/protobuf/any.proto|proto/chromium_types/any.proto|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google.protobuf.Any|Any|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google/protobuf/timestamp.proto|proto/chromium_types/timestamp.proto|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google.protobuf.Timestamp|Timestamp|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google/protobuf/duration.proto|proto/chromium_types/duration.proto|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google.protobuf.Duration|Duration|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google/protobuf/empty.proto|proto/chromium_types/empty.proto|g' "${SCRIPT_DIR}/chromium/${f}"
  sed -i 's|google.protobuf.Empty|Empty|g' "${SCRIPT_DIR}/chromium/${f}"
done
