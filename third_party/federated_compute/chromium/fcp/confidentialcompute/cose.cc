// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/confidentialcompute/cose.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "fcp/base/monitoring.h"

namespace fcp::confidential_compute {

namespace {

// CWT Claims; see https://www.iana.org/assignments/cwt/cwt.xhtml.
enum CwtClaim {
  kExp = 4,  // Expiration time.
  kIat = 6,  // Issued at.

  // Claims in the private space (-65537 and below).
  // See ../protos/confidentialcompute/cbor_ids.md for claims originating from
  // this project and
  // https://github.com/project-oak/oak/blob/main/oak_dice/src/cert.rs for Oak
  // claims.
  kPublicKey = -65537,           // Claim containing serialized public key.
  kConfigProperties = -65538,    // Claim containing configuration properties.
  kAccessPolicySha256 = -65543,  // Claim containing access policy hash.
  kOakPublicKey = -4670552,      // Oak claim containing serialized public key.
};

// COSE Header parameters; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseHeaderParameter {
  kHdrAlg = 1,
  kHdrKid = 4,

  // Parameters in the private space (-65537 and below).
  // See ../protos/confidentialcompute/cbor_ids.md.
  kEncapsulatedKey = -65537,
  kSrcState = -65538,
  kDstState = -65539,
};

// COSE Key parameters; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseKeyParameter {
  // Common parameters.
  kKty = 1,
  kKid = 2,
  kAlg = 3,
  kKeyOps = 4,

  // OKP parameters.
  kOkpCrv = -1,
  kOkpX = -2,
  kOkpD = -4,

  // Symmetric parameters.
  kSymmetricK = -1,
};

// COSE Key types; see https://www.iana.org/assignments/cose/cose.xhtml.
enum CoseKeyType {
  kOkp = 1,
  kSymmetric = 4,
};

// Builds the protected header for a COSE structure, which is a map encoded as a
// bstr.
std::vector<uint8_t> BuildProtectedHeader(
    std::optional<int64_t> algorithm,
    const std::optional<std::optional<std::string>>& src_state,
    const std::optional<std::string>& dst_state) {
  cbor::Value::MapValue map;
  if (algorithm) {
    map.emplace(cbor::Value(CoseHeaderParameter::kHdrAlg),
                cbor::Value(*algorithm));
  }
  if (src_state) {
    map.emplace(cbor::Value(CoseHeaderParameter::kSrcState),
                *src_state
                    ? cbor::Value(**src_state, cbor::Value::Type::BYTE_STRING)
                    : cbor::Value(cbor::Value::SimpleValue::NULL_VALUE));
  }
  if (dst_state) {
    map.emplace(cbor::Value(CoseHeaderParameter::kDstState),
                cbor::Value(*dst_state, cbor::Value::Type::BYTE_STRING));
  }
  std::optional<std::vector<uint8_t>> encoded_protected_header =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!encoded_protected_header) {
    return {};
  }
  return *encoded_protected_header;
}

// Builds the payload for a CWT, which is a map of CWT claims encoded as a bstr.
// See RFC 8392 section 7.1.
absl::StatusOr<std::vector<uint8_t>> BuildCwtPayload(const OkpCwt& cwt) {
  cbor::Value::MapValue map;
  if (cwt.expiration_time) {
    map.emplace(cbor::Value(CwtClaim::kExp),
                cbor::Value(absl::ToUnixSeconds(*cwt.expiration_time)));
  }
  if (cwt.issued_at) {
    map.emplace(cbor::Value(CwtClaim::kIat),
                cbor::Value(absl::ToUnixSeconds(*cwt.issued_at)));
  }
  if (cwt.public_key) {
    FCP_ASSIGN_OR_RETURN(std::string encoded_public_key,
                         cwt.public_key->Encode());
    map.emplace(
        cbor::Value(CwtClaim::kPublicKey),
        cbor::Value(encoded_public_key, cbor::Value::Type::BYTE_STRING));
  }
  if (!cwt.config_properties.empty()) {
    map.emplace(
        cbor::Value(CwtClaim::kConfigProperties),
        cbor::Value(cwt.config_properties, cbor::Value::Type::BYTE_STRING));
  }
  if (!cwt.access_policy_sha256.empty()) {
    map.emplace(
        cbor::Value(CwtClaim::kAccessPolicySha256),
        cbor::Value(cwt.access_policy_sha256, cbor::Value::Type::BYTE_STRING));
  }

  std::optional<std::vector<uint8_t>> encoded_cwt_payload =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!encoded_cwt_payload) {
    return absl::InternalError("failed to build cwt payload");
    ;
  }
  return *encoded_cwt_payload;
}

// Parses a serialized protected header from a COSE structure and sets the
// corresponding output variables (if non-null).
absl::Status ParseProtectedHeader(
    const std::vector<uint8_t>& serialized_header,
    std::optional<int64_t>* algorithm,
    std::optional<std::optional<std::string>>* src_state,
    std::optional<std::string>* dst_state) {
  std::optional<cbor::Value> header = cbor::Reader::Read(serialized_header);
  if (!header || !header->is_map()) {
    return absl::InvalidArgumentError("protected header is invalid");
  }

  // Process the parameters map.
  const cbor::Value::MapValue& map = header->GetMap();
  for (const auto& [key, value] : map) {
    if (!key.is_integer()) continue;
    switch (key.GetInteger()) {
      case CoseHeaderParameter::kHdrAlg:
        if (algorithm) {
          if (!value.is_integer()) {
            return absl::InvalidArgumentError(absl::StrCat(
                "unsupported algorithm parameter type ", value.type()));
          }
          *algorithm = value.GetInteger();
        }
        break;

      case CoseHeaderParameter::kSrcState:
        if (src_state) {
          if (value.is_simple() &&
              value.GetSimpleValue() == cbor::Value::SimpleValue::NULL_VALUE) {
            *src_state = std::optional<std::string>(std::nullopt);
          } else if (value.is_bytestring()) {
            *src_state = value.GetBytestringAsString();
          } else {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported src_state type ", value.type()));
          }
        }
        break;

      case CoseHeaderParameter::kDstState:
        if (dst_state) {
          if (!value.is_bytestring()) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported dst_state type ", value.type()));
          }
          *dst_state = value.GetBytestringAsString();
        }
        break;

      default:
        break;
    }
  }
  return absl::OkStatus();
}

// Parses a serialized CWT payload and updates the OkpCwt.
absl::Status ParseCwtPayload(const std::vector<uint8_t>& serialized_payload,
                             OkpCwt& cwt) {
  std::optional<cbor::Value> payload = cbor::Reader::Read(serialized_payload);
  if (!payload || !payload->is_map()) {
    return absl::InvalidArgumentError("cwt payload is invalid");
  }

  // Process the claims map.
  const cbor::Value::MapValue& map = payload->GetMap();
  for (const auto& [key, value] : map) {
    if (!key.is_integer()) continue;
    switch (key.GetInteger()) {
      case CwtClaim::kExp:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported exp type ", value.type()));
        }
        cwt.expiration_time = absl::FromUnixSeconds(value.GetInteger());
        break;

      case CwtClaim::kIat:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported iat type ", value.type()));
        }
        cwt.issued_at = absl::FromUnixSeconds(value.GetInteger());
        break;

      case CwtClaim::kOakPublicKey:
      case CwtClaim::kPublicKey: {
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported public_key type ", value.type()));
        }
        FCP_ASSIGN_OR_RETURN(cwt.public_key,
                             OkpKey::Decode(value.GetBytestringAsString()));
        break;
      }

      case CwtClaim::kConfigProperties:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported configuration type ", value.type()));
        }
        cwt.config_properties = value.GetBytestringAsString();
        break;

      case CwtClaim::kAccessPolicySha256:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "unsupported access_policy_sha256 type ", value.type()));
        }
        cwt.access_policy_sha256 = value.GetBytestringAsString();
        break;

      default:
        break;
    }
  }
  return absl::OkStatus();
}

// Builds a Sig_structure object for a COSE_Sign or COSE_Sign1 structure.
// See RFC 9052 section 4.4 for the contents of the Sig_structure.
std::string BuildSigStructure(
    std::vector<uint8_t> body_protected,
    std::optional<std::vector<uint8_t>> sign_protected, absl::string_view aad,
    std::vector<uint8_t> payload) {
  cbor::Value::ArrayValue sig_structure;
  sig_structure.emplace_back(sign_protected ? "Signature" : "Signature1");
  sig_structure.emplace_back(std::move(body_protected));
  if (sign_protected) {
    sig_structure.emplace_back(std::move(*sign_protected));
  }
  sig_structure.emplace_back(std::string(aad), cbor::Value::Type::BYTE_STRING);
  sig_structure.emplace_back(std::move(payload));

  std::optional<std::vector<uint8_t>> encoded_sig_structure =
      cbor::Writer::Write(cbor::Value(std::move(sig_structure)));
  if (!encoded_sig_structure) {
    return {};
  }
  return std::string(encoded_sig_structure->begin(),
                     encoded_sig_structure->end());
}

// Parses a serialized COSE_Sign or COSE_Sign1 structure and returns the
// protected header, signer protected header (COSE_Sign only), payload, and
// signature.
absl::StatusOr<
    std::tuple<std::vector<uint8_t>, std::optional<std::vector<uint8_t>>,
               std::vector<uint8_t>, std::vector<uint8_t>>>
ParseCoseSign(absl::string_view encoded) {
  std::optional<cbor::Value> value =
      cbor::Reader::Read(std::vector<uint8_t>(encoded.begin(), encoded.end()));
  if (!value || !value->is_array()) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() != 4 || !array[0].is_bytestring() || !array[1].is_map() ||
      !array[2].is_bytestring()) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  // Extract the signature and signer protected header (COSE_Sign only).
  std::optional<std::vector<uint8_t>> sign_protected;
  std::optional<std::vector<uint8_t>> signature;
  const cbor::Value& signature_component = array[3];
  if (signature_component.is_bytestring()) {
    // If the 4th element is a bstr, we're decoding a COSE_Sign1 structure.
    signature = std::move(signature_component.GetBytestring());
  } else if (signature_component.is_array() &&
             signature_component.GetArray().size() > 0) {
    // If the 4th element is an array, we're decoding a COSE_Sign structure.
    // Use the signature and protected header from the first COSE_Signature,
    // which is a (protected header, unprotected header, signature) tuple.
    const cbor::Value::ArrayValue& sigs = signature_component.GetArray();
    if (sigs[0].is_array() && sigs[0].GetArray().size() == 3 &&
        sigs[0].GetArray()[0].is_bytestring() &&
        sigs[0].GetArray()[2].is_bytestring()) {
      sign_protected = std::move(sigs[0].GetArray()[0].GetBytestring());
      signature = std::move(sigs[0].GetArray()[2].GetBytestring());
    }
  }

  if (!signature) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  return std::make_tuple(
      std::move(array[0].GetBytestring()), std::move(sign_protected),
      std::move(array[2].GetBytestring()), std::move(*signature));
}

// Builds the payload for a ReleaseToken, which is a COSE_Encrypt0 object
// encoded as a bstr. See also RFC 9052 section 5.2.
std::vector<uint8_t> BuildReleaseTokenPayload(const ReleaseToken& token) {
  cbor::Value::ArrayValue array_value;
  array_value.emplace_back(BuildProtectedHeader(
      token.encryption_algorithm, token.src_state, token.dst_state));

  cbor::Value::MapValue unprotected_header;
  if (token.encryption_key_id) {
    unprotected_header.emplace(
        CoseHeaderParameter::kHdrKid,
        cbor::Value(*token.encryption_key_id, cbor::Value::Type::BYTE_STRING));
  }
  if (token.encapped_key) {
    unprotected_header.emplace(
        CoseHeaderParameter::kEncapsulatedKey,
        cbor::Value(*token.encapped_key, cbor::Value::Type::BYTE_STRING));
  }
  array_value.emplace_back(std::move(unprotected_header));
  array_value.emplace_back(token.encrypted_payload,
                           cbor::Value::Type::BYTE_STRING);
  std::optional<std::vector<uint8_t>> encoded_array =
      cbor::Writer::Write(cbor::Value(std::move(array_value)));
  if (!encoded_array) {
    return {};
  }
  return *encoded_array;
}

// Builds a Enc_structure object for a COSE_Encrypt0 structure.
// See RFC 9052 section 5.3 for the contents of the Enc_structure.
std::string BuildEncStructure(std::vector<uint8_t> protected_header,
                              absl::string_view aad) {
  cbor::Value::ArrayValue enc_structure;
  enc_structure.emplace_back("Encrypt0");
  enc_structure.emplace_back(std::move(protected_header));
  enc_structure.emplace_back(aad, cbor::Value::Type::BYTE_STRING);
  auto result = cbor::Writer::Write(cbor::Value(std::move(enc_structure)));
  if (!result) {
    return {};
  }
  return std::string(result->begin(), result->end());
}

// Parses a serialized ReleaseToken payload and updated the ReleaseToken.
absl::StatusOr<
    std::tuple<std::vector<uint8_t>, cbor::Value, std::vector<uint8_t>>>
ParseCoseEncrypt0(const std::vector<uint8_t>& encoded) {
  std::optional<cbor::Value> value =
      cbor::Reader::Read(std::vector<uint8_t>(encoded.begin(), encoded.end()));
  if (!value || !value->is_array()) {
    return absl::InvalidArgumentError("COSE_Sign is invalid");
  }

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() != 3) {
    return absl::InvalidArgumentError(
        "COSE_Encrypt0 is invalid: not an array of size 3.");
  }

  if (!array[0].is_bytestring() || !array[1].is_map() ||
      !array[2].is_bytestring()) {
    return absl::InvalidArgumentError(
        "COSE_Encrypt0 is invalid: wrong element types.");
  }

  return std::make_tuple(array[0].GetBytestring(), array[1].Clone(),
                         array[2].GetBytestring());
}

}  // namespace

absl::StatusOr<OkpKey> OkpKey::Decode(absl::string_view encoded) {
  std::optional<cbor::Value> encoded_value =
      cbor::Reader::Read(std::vector<uint8_t>(encoded.begin(), encoded.end()));
  if (!encoded_value || !encoded_value->is_map()) {
    return absl::InvalidArgumentError("okpkey is invalid");
  }

  std::optional<int64_t> kty;
  OkpKey okp_key;
  const cbor::Value::MapValue& map = encoded_value->GetMap();
  for (const auto& [key, value] : map) {
    if (!key.is_integer()) continue;
    switch (key.GetInteger()) {
      case CoseKeyParameter::kKty:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kty type ", value.type()));
        }
        kty = value.GetInteger();
        break;

      case CoseKeyParameter::kKid:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kid type ", value.type()));
        }
        okp_key.key_id = value.GetBytestringAsString();
        break;

      case CoseKeyParameter::kAlg:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported alg type ", value.type()));
        }
        okp_key.algorithm = value.GetInteger();
        break;

      case CoseKeyParameter::kKeyOps:
        if (!value.is_array()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported key_ops type ", value.type()));
        }
        for (const cbor::Value& op : value.GetArray()) {
          if (!op.is_integer()) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported key_ops entry type", op.type()));
          }
          okp_key.key_ops.push_back(op.GetInteger());
        }
        break;

      case CoseKeyParameter::kOkpCrv:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported curve type ", value.type()));
        }
        okp_key.curve = value.GetInteger();
        break;

      case CoseKeyParameter::kOkpX:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported x type ", value.type()));
        }
        okp_key.x = value.GetBytestringAsString();
        break;

      case CoseKeyParameter::kOkpD:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported d type ", value.type()));
        }
        okp_key.d = value.GetBytestringAsString();
        break;

      default:
        break;
    }
  }

  if (!kty.has_value() || *kty != CoseKeyType::kOkp) {
    return absl::InvalidArgumentError("missing or wrong Cose_Key type");
  }
  return okp_key;
}

absl::StatusOr<std::string> OkpKey::Encode() const {
  // Generate a map containing the parameters that are set.
  cbor::Value::MapValue map;
  map.emplace(cbor::Value(CoseKeyParameter::kKty),
              cbor::Value(CoseKeyType::kOkp));
  if (!key_id.empty()) {
    map.emplace(cbor::Value(CoseKeyParameter::kKid),
                cbor::Value(key_id, cbor::Value::Type::BYTE_STRING));
  }
  if (algorithm) {
    map.emplace(cbor::Value(CoseKeyParameter::kAlg), cbor::Value(*algorithm));
  }
  if (!key_ops.empty()) {
    cbor::Value::ArrayValue array;
    for (int64_t key_op : key_ops) {
      array.emplace_back(key_op);
    }
    map.emplace(cbor::Value(CoseKeyParameter::kKeyOps),
                cbor::Value(std::move(array)));
  }
  if (curve) {
    map.emplace(cbor::Value(CoseKeyParameter::kOkpCrv), cbor::Value(*curve));
  }
  if (!x.empty()) {
    map.emplace(cbor::Value(CoseKeyParameter::kOkpX),
                cbor::Value(x, cbor::Value::Type::BYTE_STRING));
  }
  if (!d.empty()) {
    map.emplace(cbor::Value(CoseKeyParameter::kOkpD),
                cbor::Value(d, cbor::Value::Type::BYTE_STRING));
  }
  std::optional<std::vector<uint8_t>> encoded_key =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!encoded_key) {
    return absl::InternalError("failed to encode OkpKey");
  }
  return std::string(encoded_key->begin(), encoded_key->end());
}

absl::StatusOr<SymmetricKey> SymmetricKey::Decode(absl::string_view encoded) {
  std::optional<cbor::Value> symmetric_key_value =
      cbor::Reader::Read(std::vector<uint8_t>(encoded.begin(), encoded.end()));
  if (!symmetric_key_value || !symmetric_key_value->is_map()) {
    return absl::InvalidArgumentError("symmetric key is invalid");
  }

  std::optional<int64_t> kty;
  SymmetricKey symmetric_key;
  const cbor::Value::MapValue& map = symmetric_key_value->GetMap();
  for (const auto& [key, value] : map) {
    if (!key.is_integer()) continue;
    switch (key.GetInteger()) {
      case CoseKeyParameter::kKty:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported kty type ", value.type()));
        }
        kty = value.GetInteger();
        break;

      case CoseKeyParameter::kAlg:
        if (!value.is_integer()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported alg type ", value.type()));
        }
        symmetric_key.algorithm = value.GetInteger();
        break;

      case CoseKeyParameter::kKeyOps:
        if (!value.is_array()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported key_ops type ", value.type()));
        }
        for (const cbor::Value& op : value.GetArray()) {
          if (!op.is_integer()) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported key_ops entry type", op.type()));
          }
          symmetric_key.key_ops.push_back(op.GetInteger());
        }
        break;

      case CoseKeyParameter::kSymmetricK:
        if (!value.is_bytestring()) {
          return absl::InvalidArgumentError(
              absl::StrCat("unsupported k type ", value.type()));
        }
        symmetric_key.k = value.GetBytestringAsString();
        break;

      default:
        break;
    }
  }

  if (!kty.has_value() || *kty != CoseKeyType::kSymmetric) {
    return absl::InvalidArgumentError("missing or wrong Cose_Key type");
  }
  return symmetric_key;
}

absl::StatusOr<std::string> SymmetricKey::Encode() const {
  cbor::Value::MapValue map;
  map.emplace(cbor::Value(CoseKeyParameter::kKty),
              cbor::Value(CoseKeyType::kSymmetric));
  if (algorithm) {
    map.emplace(cbor::Value(CoseKeyParameter::kAlg), cbor::Value(*algorithm));
  }
  if (!key_ops.empty()) {
    cbor::Value::ArrayValue array;
    for (int64_t key_op : key_ops) {
      array.emplace_back(key_op);
    }
    map.emplace(cbor::Value(CoseKeyParameter::kKeyOps),
                cbor::Value(std::move(array)));
  }
  if (!k.empty()) {
    map.emplace(cbor::Value(CoseKeyParameter::kSymmetricK),
                cbor::Value(k, cbor::Value::Type::BYTE_STRING));
  }
  std::optional<std::vector<uint8_t>> encoded_key =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!encoded_key) {
    return absl::InternalError("failed to encode SymmetricKey");
  }
  return std::string(encoded_key->begin(), encoded_key->end());
}

absl::StatusOr<std::string> OkpCwt::BuildSigStructureForSigning(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(algorithm, /*src_state=*/std::nullopt,
                           /*dst_state=*/std::nullopt);
  FCP_ASSIGN_OR_RETURN(std::vector<uint8_t> payload, BuildCwtPayload(*this));
  return BuildSigStructure(std::move(protected_header), std::nullopt, aad,
                           std::move(payload));
}

absl::StatusOr<std::string> OkpCwt::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  std::vector<uint8_t> body_protected, payload;
  std::optional<std::vector<uint8_t>> sign_protected;
  FCP_ASSIGN_OR_RETURN(
      std::tie(body_protected, sign_protected, payload, std::ignore),
      ParseCoseSign(encoded));
  return BuildSigStructure(std::move(body_protected), std::move(sign_protected),
                           aad, std::move(payload));
}

absl::StatusOr<OkpCwt> OkpCwt::Decode(absl::string_view encoded) {
  std::vector<uint8_t> body_protected, payload, signature;
  std::optional<std::vector<uint8_t>> sign_protected;
  FCP_ASSIGN_OR_RETURN(
      std::tie(body_protected, sign_protected, payload, signature),
      ParseCoseSign(encoded));
  OkpCwt cwt;
  // When decoding a COSE_Sign structure, information will be in the signer
  // protected header instead of the body protected header.
  FCP_RETURN_IF_ERROR(ParseProtectedHeader(
      sign_protected ? *sign_protected : body_protected, &cwt.algorithm,
      /*src_state=*/nullptr,
      /*dst_state=*/nullptr));
  FCP_RETURN_IF_ERROR(ParseCwtPayload(payload, cwt));
  cwt.signature = std::string(signature.begin(), signature.end());
  return cwt;
}

absl::StatusOr<std::string> OkpCwt::Encode() const {
  // See RFC 9052 section 4.2 for the contents of the COSE_Sign1 structure.
  cbor::Value::ArrayValue array;
  array.emplace_back(BuildProtectedHeader(algorithm, /*src_state=*/std::nullopt,
                                          /*dst_state=*/std::nullopt));
  array.emplace_back(cbor::Value::MapValue());  // unprotected header
  FCP_ASSIGN_OR_RETURN(std::vector<uint8_t> payload, BuildCwtPayload(*this));
  array.emplace_back(std::move(payload));
  array.emplace_back(signature, cbor::Value::Type::BYTE_STRING);
  std::optional<std::vector<uint8_t>> encoded_array =
      cbor::Writer::Write(cbor::Value(std::move(array)));
  if (!encoded_array) {
    return absl::InternalError("failed to encode OkpCwt");
  }
  return std::string(encoded_array->begin(), encoded_array->end());
}

absl::StatusOr<std::string> ReleaseToken::BuildEncStructureForEncrypting(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(encryption_algorithm, src_state, dst_state);
  return BuildEncStructure(std::move(protected_header), aad);
}

absl::StatusOr<std::string> ReleaseToken::GetEncStructureForDecrypting(
    absl::string_view encoded, absl::string_view aad) {
  std::vector<uint8_t> payload;
  FCP_ASSIGN_OR_RETURN(std::tie(std::ignore, std::ignore, payload, std::ignore),
                       ParseCoseSign(encoded));
  std::vector<uint8_t> protected_header;
  FCP_ASSIGN_OR_RETURN(std::tie(protected_header, std::ignore, std::ignore),
                       ParseCoseEncrypt0(payload));
  return BuildEncStructure(std::move(protected_header), aad);
}

absl::StatusOr<std::string> ReleaseToken::BuildSigStructureForSigning(
    absl::string_view aad) const {
  std::vector<uint8_t> protected_header =
      BuildProtectedHeader(signing_algorithm, /*src_state=*/std::nullopt,
                           /*dst_state=*/std::nullopt);
  std::vector<uint8_t> payload = BuildReleaseTokenPayload(*this);
  return BuildSigStructure(std::move(protected_header), std::nullopt, aad,
                           std::move(payload));
}

absl::StatusOr<std::string> ReleaseToken::GetSigStructureForVerifying(
    absl::string_view encoded, absl::string_view aad) {
  // Like a CWT, a ReleaseToken is also a COSE_Sign1 object, so the
  // Sig_structure is the same.
  return OkpCwt::GetSigStructureForVerifying(encoded, aad);
}

absl::StatusOr<ReleaseToken> ReleaseToken::Decode(absl::string_view encoded) {
  ReleaseToken token;

  // Parse the outer COSE_Sign1 structure.
  std::vector<uint8_t> protected_header_sign, signature;
  std::optional<std::vector<uint8_t>> payload_sign;
  FCP_ASSIGN_OR_RETURN(
      std::tie(protected_header_sign, std::ignore, payload_sign, signature),
      ParseCoseSign(encoded));

  if (!payload_sign) {
    return absl::InternalError(
        "empty payload sign when decoding release token");
  }

  FCP_RETURN_IF_ERROR(
      ParseProtectedHeader(protected_header_sign, &token.signing_algorithm,
                           /*src_state=*/nullptr, /*dst_state=*/nullptr));
  token.signature = std::string(signature.begin(), signature.end());

  // Parse the inner COSE_Encrypt0 structure.
  std::vector<uint8_t> protected_header_encrypt, encrypted_payload;
  cbor::Value unprotected_header;
  FCP_ASSIGN_OR_RETURN(
      std::tie(protected_header_encrypt, unprotected_header, encrypted_payload),
      ParseCoseEncrypt0(*payload_sign));

  FCP_RETURN_IF_ERROR(ParseProtectedHeader(protected_header_encrypt,
                                           &token.encryption_algorithm,
                                           &token.src_state, &token.dst_state));
  token.encrypted_payload =
      std::string(encrypted_payload.begin(), encrypted_payload.end());

  // Process the COSE_Encrypt0 unprotected header.
  if (unprotected_header.is_map()) {
    for (const auto& [key, value] : unprotected_header.GetMap()) {
      if (!key.is_integer()) {
        continue;  // Ignore other key types.
      }
      switch (key.GetInteger()) {
        case CoseHeaderParameter::kHdrKid:
          if (!value.is_bytestring()) {
            return absl::InvalidArgumentError(
                absl::StrCat("unsupported kid type ", value.type()));
          }
          token.encryption_key_id = value.GetBytestringAsString();
          break;

        case CoseHeaderParameter::kEncapsulatedKey:
          if (!value.is_bytestring()) {
            return absl::InvalidArgumentError(absl::StrCat(
                "unsupported encapsulated key type ", value.type()));
          }
          token.encapped_key = value.GetBytestringAsString();
          break;

        default:
          break;
      }
    }
  }
  return token;
}

absl::StatusOr<std::string> ReleaseToken::Encode() const {
  // See RFC 9052 section 4.2 for the contents of the COSE_Sign1 structure.
  cbor::Value::ArrayValue array;
  array.emplace_back(BuildProtectedHeader(signing_algorithm,
                                          /*src_state=*/std::nullopt,
                                          /*dst_state=*/std::nullopt));
  array.emplace_back(cbor::Value::MapValue());  // unprotected header
  array.emplace_back(BuildReleaseTokenPayload(*this));
  array.emplace_back(signature, cbor::Value::Type::BYTE_STRING);
  std::optional<std::vector<uint8_t>> encoded_array =
      cbor::Writer::Write(cbor::Value(std::move(array)));
  if (!encoded_array) {
    return absl::InternalError("failed to build release token");
  }
  return std::string(encoded_array->begin(), encoded_array->end());
}

}  // namespace fcp::confidential_compute