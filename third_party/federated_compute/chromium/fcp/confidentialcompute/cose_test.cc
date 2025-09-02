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

#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/testing/testing.h"
#include "gmock/gmock.h"
#include "google/protobuf/struct.pb.h"
#include "gtest/gtest.h"

namespace fcp::confidential_compute {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

MATCHER_P(IsOkAndHolds, matcher, "") {
  return arg.ok() &&
         testing::ExplainMatchResult(matcher, arg.value(), result_listener);
}

TEST(OkpKeyTest, EncodeEmpty) {
  EXPECT_THAT(OkpKey().Encode(),
              IsOkAndHolds("\xa1"      // map with 1 item:
                           "\x01\x01"  // key type (1): OKP (1)
                           ));
}

TEST(OkpKeyTest, EncodeFull) {
  EXPECT_THAT((OkpKey{
                   .key_id = "key-id",
                   .algorithm = 7,
                   .key_ops = {1, 2},
                   .curve = 45,
                   .x = "x-value",
                   .d = "d-value",
               })
                  .Encode(),
              IsOkAndHolds("\xa7"                // map with 7 items:
                           "\x01\x01"            // key type (1): OKP (1)
                           "\x02\x46key-id"      // key id (2): b"key-id"
                           "\x03\x07"            // algorithm (3): 7
                           "\x04\x82\x01\x02"    // key_ops (4): [1, 2]
                           "\x20\x18\x2d"        // curve (-1): 45
                           "\x21\x47x-value"     // x (-2): b"x-value"
                           "\x23\x47\x64-value"  // d (-4): b"d-value"
                           ));
}

TEST(OkpKeyTest, DecodeEmpty) {
  absl::StatusOr<OkpKey> key = OkpKey::Decode("\xa1\x01\x01");
  ASSERT_OK(key);
  EXPECT_EQ(key->key_id, "");
  EXPECT_EQ(key->algorithm, std::nullopt);
  EXPECT_THAT(key->key_ops, IsEmpty());
  EXPECT_EQ(key->curve, std::nullopt);
  EXPECT_EQ(key->x, "");
  EXPECT_EQ(key->d, "");
}

TEST(OkpKeyTest, DecodeFull) {
  absl::StatusOr<OkpKey> key = OkpKey::Decode(
      "\xa7\x01\x01\x02\x46key-id\x03\x07\x04\x82\x01\x02\x20\x18\x2d\x21\x47x-"
      "value\x23\x47\x64-value");
  ASSERT_OK(key);
  EXPECT_EQ(key->key_id, "key-id");
  EXPECT_EQ(key->algorithm, 7);
  EXPECT_THAT(key->key_ops, ElementsAre(1, 2));
  EXPECT_EQ(key->curve, 45);
  EXPECT_EQ(key->x, "x-value");
  EXPECT_EQ(key->d, "d-value");
}

TEST(OkpKeyTest, DecodeInvalid) {
  EXPECT_THAT(OkpKey::Decode(""), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpKey::Decode("\xa5"),  // map with 5 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpKey::Decode("\xa0 extra"),  // map with 0 items + " extra"
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(SymmetricKeyTest, EncodeEmpty) {
  EXPECT_THAT(SymmetricKey().Encode(),
              IsOkAndHolds("\xa1"      // map with 1 item:
                           "\x01\x04"  // key type (1): Symmetric (4)
                           ));
}

TEST(SymmetricKeyTest, EncodeFull) {
  EXPECT_THAT((SymmetricKey{
                   .algorithm = 7,
                   .key_ops = {1, 2},
                   .k = "secret",
               })
                  .Encode(),
              IsOkAndHolds("\xa4"              // map with 4 items:
                           "\x01\x04"          // key type (1): Symmetric (4)
                           "\x03\x07"          // algorithm (3): 7
                           "\x04\x82\x01\x02"  // key_ops (4): [1, 2]
                           "\x20\x46secret"    // k (-1): b"secret"
                           ));
}

TEST(SymmetricKeyTest, DecodeEmpty) {
  absl::StatusOr<SymmetricKey> key = SymmetricKey::Decode("\xa1\x01\x04");
  ASSERT_OK(key);
  EXPECT_EQ(key->algorithm, std::nullopt);
  EXPECT_THAT(key->key_ops, IsEmpty());
  EXPECT_EQ(key->k, "");
}

TEST(SymmetricKeyTest, DecodeFull) {
  absl::StatusOr<SymmetricKey> key = SymmetricKey::Decode(
      "\xa4\x01\x04\x03\x07\x04\x82\x01\x02\x20\x46secret");
  ASSERT_OK(key);
  EXPECT_EQ(key->algorithm, 7);
  EXPECT_THAT(key->key_ops, ElementsAre(1, 2));
  EXPECT_EQ(key->k, "secret");
}

TEST(SymmetricKeyTest, DecodeInvalid) {
  EXPECT_THAT(SymmetricKey::Decode(""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(SymmetricKey::Decode("\xa3"),  // map with 5 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(SymmetricKey::Decode("\xa0 xtra"),  // map with 0 items + " xtra"
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(OkpCwtTest, BuildSigStructureForSigningEmpty) {
  EXPECT_THAT(OkpCwt().BuildSigStructureForSigning(""),
              IsOkAndHolds("\x84"            // array with 4 items:
                           "\x6aSignature1"  // "Signature1"
                           "\x41\xa0"        // protected headers
                           "\x40"            // aad: b""
                           "\x41\xa0"        // payload (empty claims map)
                           ));
}

TEST(OkpCwtTest, BuildSigStructureForSigningFull) {
  EXPECT_THAT(
      (OkpCwt{
           .algorithm = 7,
           .issued_at = absl::FromUnixSeconds(1000),
           .expiration_time = absl::FromUnixSeconds(2000),
           .public_key = OkpKey(),
           .signature = "signature",
       })
          .BuildSigStructureForSigning("aad"),
      IsOkAndHolds(absl::string_view(
          "\x84"            // array with 4 items:
          "\x6aSignature1"  // "Signature1"
          "\x43\xa1"        // protected headers: bstr map w/ 1 item
          "\x01\x07"        // alg: 7
          "\x43"            // associated data: 3-byte string
          "aad"
          "\x52\xa3"          // payload: bstr map w/ 3 items (claims)
          "\x04\x19\x07\xd0"  // expiration time (4) = 2000
          "\x06\x19\x03\xe8"  // issued at (6) = 1000
          "\x3a\x00\x01\x00\x00\x43\xa1\x01\x01",  // public key (-65537)
                                                   // = empty OkpKey
          39)));
}

TEST(OkpCwtTest, GetSigStructureForVerifyingMatchesStructureForSigning) {
  OkpCwt cwt{
      .algorithm = 7,
      .issued_at = absl::FromUnixSeconds(1000),
      .expiration_time = absl::FromUnixSeconds(2000),
      .public_key = OkpKey(),
      .signature = "signature",
  };
  absl::StatusOr<std::string> expected = cwt.BuildSigStructureForSigning("aad");
  ASSERT_OK(expected);
  absl::StatusOr<std::string> encoded = cwt.Encode();
  ASSERT_OK(encoded);

  absl::StatusOr<std::string> sig_structure =
      OkpCwt::GetSigStructureForVerifying(*encoded, "aad");
  ASSERT_OK(sig_structure);
  EXPECT_EQ(*sig_structure, *expected);
}

TEST(OkpCwtTest, GetSigStructureForVerifyingInvalid) {
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying("", ""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying("\xa3",  // map with 3 items
                                                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying(
                  "\xa0 extra",  // map with 0 items + " extra"
                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));

  // Even if the CBOR is valid, the top-level structure must be a 4-element
  // array.
  EXPECT_THAT(
      OkpCwt::GetSigStructureForVerifying("\xa0", ""),  // map with 0 items
      IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      OkpCwt::GetSigStructureForVerifying("\x80", ""),  // array with 0 items
      IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying(
                  "\x83\x41\xa0\xa0\x41\xa0",  // array with 3 items
                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying(
                  "\x85\x41\xa0\xa0\x41\xa0\x40\x40",  // array with 5 items
                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));

  // The map entry types must be bstr, *, bstr, *.
  // "\x84\x41\xa0\xa0\x41\xa0\x40" is valid.
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying(
                  "\x84\xa0\xa0\x41\xa0\x40",  // 1st not bstr
                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::GetSigStructureForVerifying(
                  "\x84\x41\xa0\xa0\xa0\x40",  // 3rd not bstr
                  ""),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(OkpCwtTest, GetSigStructureForVerifyingReferenceExample) {
  // From RFC 8392 Section A.3, without the leading COSE_Sign1 tag "d2":
  std::string encoded = "";
  EXPECT_TRUE(absl::HexStringToBytes(
      "8443a10126a104524173796d6d657472696345434453413235365850a701756"
      "36f61703a2f2f61732e6578616d706c652e636f6d02656572696b77037818636f"
      "61703a2f2f6c696768742e6578616d706c652e636f6d041a5612aeb0051a5610d"
      "9f0061a5610d9f007420b7158405427c1ff28d23fbad1f29c4c7c6a555e601d6f"
      "a29f9179bc3d7438bacaca5acd08c8d4d4f96131680c429a01f85951ecee743a5"
      "2b9b63632c57209120e1c9e30",
      &encoded));
  absl::StatusOr<std::string> sig_structure =
      OkpCwt::GetSigStructureForVerifying(encoded, "aad");
  ASSERT_OK(sig_structure);
  EXPECT_EQ(
      absl::BytesToHexString(*sig_structure),
      "846a5369676e61747572653143a10126436161645850a70175636f61703a2f2f61732e65"
      "78616d706c652e636f6d02656572696b77037818636f61703a2f2f6c696768742e657861"
      "6d706c652e636f6d041a5612aeb0051a5610d9f0061a5610d9f007420b71");
}

TEST(OkpCwtTest, EncodeEmpty) {
  EXPECT_THAT(
      OkpCwt().Encode(),
      IsOkAndHolds("\x84"      // array with 4 items:
                   "\x41\xa0"  // bstr containing empty map (protected headers)
                   "\xa0"      // empty map (unprotected headers)
                   "\x41\xa0"  // bstr containing empty map (claims)
                   "\x40"      // b"" (signature)
                   ));
}

TEST(OkpCwtTest, EncodeFull) {
  google::protobuf::Struct config_properties;
  (*config_properties.mutable_fields())["x"].set_bool_value(true);

  EXPECT_THAT(
      (OkpCwt{
           .algorithm = 7,
           .issued_at = absl::FromUnixSeconds(1000),
           .expiration_time = absl::FromUnixSeconds(2000),
           .public_key = OkpKey(),
           .config_properties = config_properties.SerializeAsString(),
           .access_policy_sha256 = "hash",
           .signature = "signature",
       })
          .Encode(),
      IsOkAndHolds(absl::string_view(
          "\x84"              // array with 4 items:
          "\x43\xa1\x01\x07"  // bstr containing { alg: 7 } (protected headers)
          "\xa0"              // empty map (unprotected headers)
          "\x58\x2b\xa5"      // bstr containing a map with 5 items: (claims)
          "\x04\x19\x07\xd0"  // expiration time (4) = 2000
          "\x06\x19\x03\xe8"  // issued at (6) = 1000
          "\x3a\x00\x01\x00\x00\x43\xa1\x01\x01"  // public key (-65537)
                                                  // = empty OkpKey
          "\x3a\x00\x01\x00\x01\x49"              // config (-65538) = {
          "\x0a\x07"                              // fields (1) {
          "\x0a\x01x"                             //   key (1): "x"
          "\x12\x02"                              //   value (2): {
          "\x20\x01"                              //     bool_value (4): true
                                                  //   }
                                                  // }
          "\x3a\x00\x01\x00\x06\x44hash"  // access_policy_sha256 (-65543) =
                                          // b"hash"
          "\x49signature",
          61)));
}

TEST(OkpCwtTest, DecodeEmpty) {
  absl::StatusOr<OkpCwt> key = OkpCwt::Decode("\x84\x41\xa0\xa0\x41\xa0\x40");
  ASSERT_OK(key);
  EXPECT_EQ(key->issued_at, std::nullopt);
  EXPECT_EQ(key->expiration_time, std::nullopt);
  EXPECT_EQ(key->public_key, std::nullopt);
  EXPECT_EQ(key->config_properties, "");
  EXPECT_EQ(key->access_policy_sha256, "");
  EXPECT_EQ(key->signature, "");
}

TEST(OkpCwtTest, DecodeFull) {
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(absl::string_view(
      "\x84\x43\xa1\x01\x07\xa0\x58\x2b\xa5\x04\x19\x07\xd0\x06\x19\x03\xe8\x3a"
      "\x00\x01\x00\x00\x43\xa1\x01\x01\x3a\x00\x01\x00\x01\x49\x0a\x07\x0a\x01"
      "x\x12\x02\x20\x01\x3a\x00\x01\x00\x06\x44hash\x49signature",
      61));
  ASSERT_OK(cwt);
  EXPECT_EQ(cwt->issued_at, absl::FromUnixSeconds(1000));
  EXPECT_EQ(cwt->expiration_time, absl::FromUnixSeconds(2000));
  EXPECT_TRUE(cwt->public_key.has_value());
  EXPECT_EQ(cwt->access_policy_sha256, "hash");
  EXPECT_EQ(cwt->signature, "signature");

  google::protobuf::Struct config_properties;
  ASSERT_TRUE(config_properties.ParseFromString(cwt->config_properties));

  // Because `google/protobuf/util/message_differencer.h` cannot be used with
  // PROTOBUF_EXPORT in Chromium, we cannot use EqualsProto. As such, we
  // manually match the expected fields.

  // EXPECT_THAT(config_properties, EqualsProto(R"pb(
  //               fields {
  //                 key: "x"
  //                 value { bool_value: true }
  //               }
  //             )pb"));

  const auto& fields = config_properties.fields();
  ASSERT_EQ(fields.size(), 1u);

  const auto it = fields.find("x");
  ASSERT_NE(it, fields.end());

  const auto& value = it->second;
  ASSERT_EQ(value.kind_case(), google::protobuf::Value::kBoolValue);
  ASSERT_TRUE(value.bool_value());
}

TEST(OkpCwtTest, DecodeInvalid) {
  EXPECT_THAT(OkpCwt::Decode(""), IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\xa3"),  // map with 5 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\xa0 extra"),  // map with 0 items + " extra"
              IsCode(absl::StatusCode::kInvalidArgument));

  // Even if the CBOR is valid, the top-level structure must be a 4-element
  // array.
  EXPECT_THAT(OkpCwt::Decode("\xa0"),  // map with 0 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\x80"),  // array with 0 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\x83\x41\xa0\xa0\x41\xa0"),  // array with 3 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      OkpCwt::Decode("\x85\x41\xa0\xa0\x41\xa0\x40\x40"),  // array with 5 items
      IsCode(absl::StatusCode::kInvalidArgument));

  // The map entry types must be bstr, map, bstr, bstr.
  // "\x84\x41\xa0\xa0\x41\xa0\x40" is valid.
  EXPECT_THAT(OkpCwt::Decode("\x84\xa0\xa0\x41\xa0\x40"),  // 1st not bstr
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\x84\x41\xa0\x40\x41\xa0\x40"),  // 2nd not map
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\x84\x41\xa0\xa0\xa0\x40"),  // 3rd not bstr
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(OkpCwt::Decode("\x84\x41\xa0\xa0\x41\xa0\xa0"),  // 4th not bstr
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(OkpCwtTest, DecodeCoseSign1ReferenceExample) {
  // From RFC 8392 Section A.3, without the leading COSE_Sign1 tag "d2":
  std::string encoded = "";
  EXPECT_TRUE(absl::HexStringToBytes(
      "8443a10126a104524173796d6d657472696345434453413235365850a701756"
      "36f61703a2f2f61732e6578616d706c652e636f6d02656572696b77037818636f"
      "61703a2f2f6c696768742e6578616d706c652e636f6d041a5612aeb0051a5610d"
      "9f0061a5610d9f007420b7158405427c1ff28d23fbad1f29c4c7c6a555e601d6f"
      "a29f9179bc3d7438bacaca5acd08c8d4d4f96131680c429a01f85951ecee743a5"
      "2b9b63632c57209120e1c9e30",
      &encoded));
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(encoded);
  ASSERT_OK(cwt);
  EXPECT_EQ(cwt->issued_at, absl::FromUnixSeconds(1443944944));
  EXPECT_EQ(cwt->expiration_time, absl::FromUnixSeconds(1444064944));
  EXPECT_FALSE(cwt->public_key.has_value());
  std::string expected_signature = "";
  EXPECT_TRUE(absl::HexStringToBytes(
      "5427c1ff28d23fbad1f29c4c7c6a555e601d6fa29f9179bc3d7438bacaca5acd08c8"
      "d4d4f96131680c429a01f85951ecee743a52b9b63632c57209120e1c9e30",
      &expected_signature));
  EXPECT_EQ(cwt->signature, expected_signature);
}

TEST(OkpCwtTest, VerifyAndDecodeCoseSign) {
  // The encoded CWT and signature structure were generated using the Rust coset
  // crate.
  std::string encoded = "";
  EXPECT_TRUE(absl::HexStringToBytes(
      "8440a05846a30419162e061904d23a000100005836a5010102466b65792d6964033a0001"
      "000020042158204850e21e94eb470337fd46a401f4c8b46150195732fb47d53fa0533f59"
      "4cb342828343a10126a058208dfb544c010408b5c24eeaf67e2ff89b98dcab365d50b244"
      "7d569c1561540bc28344a101382ea058206b33008400add41a3b82ef4bf4bb85fcf8d2d5"
      "7d2f453bcddb277cf3fa3880d6",
      &encoded));

  absl::StatusOr<std::string> sig_structure =
      OkpCwt::GetSigStructureForVerifying(encoded, "");
  ASSERT_OK(sig_structure);
  EXPECT_EQ(
      absl::BytesToHexString(*sig_structure),
      "85695369676e61747572654043a10126405846a30419162e061904d23a000100005836a5"
      "010102466b65792d6964033a0001000020042158204850e21e94eb470337fd46a401f4c8"
      "b46150195732fb47d53fa0533f594cb342");

  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(encoded);
  ASSERT_OK(cwt);
  EXPECT_EQ(cwt->algorithm, -7);
  EXPECT_EQ(cwt->issued_at, absl::FromUnixSeconds(1234));
  EXPECT_EQ(cwt->expiration_time, absl::FromUnixSeconds(5678));
  ASSERT_TRUE(cwt->public_key.has_value());
  EXPECT_EQ(cwt->public_key->key_id, "key-id");
  EXPECT_EQ(cwt->public_key->algorithm, -65537);
  EXPECT_EQ(cwt->public_key->curve, 4);
  EXPECT_NE(cwt->public_key->x, "");
}
TEST(ReleaseTokenTest, EncodeEmpty) {
  EXPECT_THAT(
      ReleaseToken().Encode(),
      IsOkAndHolds(
          "\x84"      // array with 4 items:
          "\x41\xa0"  // bstr containing empty map (protected headers)
          "\xa0"      // empty map (unprotected headers)
          "\x45\x83"  // bstr containing an array with 3 items: (COSE_Encrypt0)
          "\x41\xa0"  // bstr containing empty map (protected headers)
          "\xa0"      // empty map (unprotected headers)
          "\x40"      // b"" (encrypted payload)
          "\x40"      // b"" (signature)
          ));
}

TEST(ReleaseTokenTest, EncodeFull) {
  EXPECT_THAT(
      (ReleaseToken{
           .signing_algorithm = 1,
           .encryption_algorithm = 2,
           .encryption_key_id = "key-id",
           .src_state = "old-state",
           .dst_state = "new-state",
           .encrypted_payload = "payload",
           .encapped_key = "key",
           .signature = "signature",
       })
          .Encode(),
      IsOkAndHolds(absl::string_view(
          "\x84"              // array with 4 items:
          "\x43\xa1\x01\x01"  // bstr containing { alg: 1 } (protected headers)
          "\xa0"              // empty map (unprotected headers)
          "\x58\x3e\x83"      // bstr containing an array with 3 items:
                              // (COSE_Encrypt0)
          "\x58\x21\xa3"      // bstr containing a map with 3 items:
                              // (protected headers)
          "\x01\x02"          // alg: 2
          "\x3a\x00\x01\x00\x01\x49old-state"  // -65538: b"old-state"
          "\x3a\x00\x01\x00\x02\x49new-state"  // -65539: b"new-state"
          "\xa2"            // map with 2 items: (unprotected headers)
          "\x04\x46key-id"  // key_id: b"key-id"
          "\x3a\x00\x01\x00\x00\x43key"  // encapped_key: b"key"
          "\x47payload"                  // b"encrypted-payload" (payload)
          "\x49signature",               // b"signature" (signature)
          80)));
}

TEST(ReleaseTokenTest, EncodeNullSrcState) {
  EXPECT_THAT(
      (ReleaseToken{
           .src_state = std::optional<std::string>(std::nullopt),
       })
          .Encode(),
      IsOkAndHolds(absl::string_view(
          "\x84"      // array with 4 items:
          "\x41\xa0"  // bstr containing empty map (protected headers)
          "\xa0"      // empty map (unprotected headers)
          "\x4b\x83"  // bstr containing an array with 3 items:
                      // (COSE_Encrypt0)
          "\x47\xa1\x3a\x00\x01\x00\x01\xf6"  // bstr containing { src_state:
                                              // null } (protected headers)
          "\xa0"                              // empty map (unprotected headers)
          "\x40"                              // b"" (payload)
          "\x40",                             // b"" (signature)
          17)));
}

TEST(ReleaseTokenTest, DecodeEmpty) {
  absl::StatusOr<ReleaseToken> release_token =
      ReleaseToken::Decode("\x84\x41\xa0\xa0\x45\x83\x41\xa0\xa0\x40\x40");
  ASSERT_OK(release_token);
  EXPECT_EQ(release_token->signing_algorithm, std::nullopt);
  EXPECT_EQ(release_token->encryption_algorithm, std::nullopt);
  EXPECT_EQ(release_token->encryption_key_id, std::nullopt);
  EXPECT_EQ(release_token->src_state, std::nullopt);
  EXPECT_EQ(release_token->dst_state, std::nullopt);
  EXPECT_EQ(release_token->encrypted_payload, "");
  EXPECT_EQ(release_token->encapped_key, std::nullopt);
  EXPECT_EQ(release_token->signature, "");
}

TEST(ReleaseTokenTest, DecodeFull) {
  absl::StatusOr<ReleaseToken> release_token =
      ReleaseToken::Decode(absl::string_view(
          "\x84\x43\xa1\x01\x01\xa0\x58\x3e\x83\x58\x21\xa3\x01\x02\x3a\x00\x01"
          "\x00\x01\x49old-state\x3a\x00\x01\x00\x02\x49new-state\xa2\x04\x46ke"
          "y-id\x3a\x00\x01\x00\x00\x43key\x47payload\x49signature",
          80));
  ASSERT_OK(release_token);
  EXPECT_EQ(release_token->signing_algorithm, 1);
  EXPECT_EQ(release_token->encryption_algorithm, 2);
  EXPECT_EQ(release_token->encryption_key_id, "key-id");
  EXPECT_EQ(release_token->src_state, "old-state");
  EXPECT_EQ(release_token->dst_state, "new-state");
  EXPECT_EQ(release_token->encrypted_payload, "payload");
  EXPECT_EQ(release_token->encapped_key, "key");
  EXPECT_EQ(release_token->signature, "signature");
}

TEST(ReleaseTokenTest, DecodeNullSrcState) {
  absl::StatusOr<ReleaseToken> release_token = ReleaseToken::Decode(
      absl::string_view("\x84\x41\xa0\xa0\x4b\x83\x47\xa1\x3a\x00\x01\x00\x01"
                        "\xf6\xa0\x40\x40",
                        17));
  ASSERT_OK(release_token);
  ASSERT_TRUE(release_token->src_state.has_value());
  EXPECT_EQ(*release_token->src_state, std::nullopt);
}

TEST(ReleaseTokenTest, DecodeInvalidCoseSign1) {
  EXPECT_THAT(ReleaseToken::Decode(""),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode("\xa3"),  // map with 3 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      ReleaseToken::Decode("\xa0 extra"),  // map with 0 items + " extra"
      IsCode(absl::StatusCode::kInvalidArgument));

  // Even if the CBOR is valid, the top-level structure must be a 4-element
  // array.
  EXPECT_THAT(ReleaseToken::Decode("\xa0"),  // map with 0 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode("\x80"),  // array with 0 items
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      ReleaseToken::Decode("\x83\x41\xa0\xa0\x41\xa0"),  // array with 3 items
      IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // array with 5 items
                  "\x85\x41\xa0\xa0\x41\xa0\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));

  // The COSE_Sign1 array entry types must be bstr, map, bstr, bstr.
  // "\x84\x41\xa0\xa0\x45\x83\x41\xa0\xa0\x40\x40" is valid.
  EXPECT_THAT(ReleaseToken::Decode(  // 1st not bstr
                  "\x84\xa0\xa0\x45\x83\x41\xa0\xa0\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // 2nd not map
                  "\x84\x41\xa0\x40\x45\x83\x41\xa0\xa0\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode("\x84\x41\xa0\xa0\xa0\x40"),  // 3rd not bstr
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // 4th not bstr
                  "\x84\x41\xa0\xa0\x45\x83\x41\xa0\xa0\x40\xa0"),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ReleaseTokenTest, DecodeInvalidCoseEncrypt0) {
  // The serialized COSE_Encrypt0 is wrapped in a COSE_Sign1 structure:
  // "\x84\x41\xa0\xa0\x4N<COSE_Encrypt0>\x40" is valid.

  EXPECT_THAT(ReleaseToken::Decode("\x84\x41\xa0\xa0\x40\x40"),  // empty string
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      ReleaseToken::Decode("\x84\x41\xa0\xa0\x41\xa3\x40"),  // map with 3 items
      IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // map with 0 items + " extra"
                  "\x84\x41\xa0\xa0\x47\xa0 extra\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));

  // Even if the CBOR is valid, the top-level structure must be a 4-element
  // array.
  EXPECT_THAT(
      ReleaseToken::Decode("\x84\x41\xa0\xa0\x41\xa0\x40"),  // map with 0 items
      IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // array with 0 items
                  "\x84\x41\xa0\xa0\x41\x80\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // array with 2 items
                  "\x84\x41\xa0\xa0\x44\x82\x41\xa0\xa0\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // array with 4 items
                  "\x84\x41\xa0\xa0\x46\x84\x41\xa0\xa0\x40\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));

  // The COSE_Encrypt0 array entry types must be bstr, map, bstr.
  // "\x84\x41\xa0\xa0\x45\x83\x41\xa0\xa0\x40\x40" is valid.
  EXPECT_THAT(ReleaseToken::Decode(  // 1st not bstr
                  "\x84\x41\xa0\xa0\x44\x83\xa0\xa0\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // 2nd not map
                  "\x84\x41\xa0\xa0\x45\x83\x41\xa0\x40\x40\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ReleaseToken::Decode(  // 3rd not bstr
                  "\x84\x41\xa0\xa0\x45\x83\x41\xa0\xa0\xa0\x40"),
              IsCode(absl::StatusCode::kInvalidArgument));
}

TEST(ReleaseTokenTest, BuildEncStructureForEncrypting) {
  EXPECT_THAT((ReleaseToken{
                   .signing_algorithm = 1,
                   .encryption_algorithm = 2,
                   .encryption_key_id = "key-id",
                   .src_state = "old-state",
                   .dst_state = "new-state",
                   .encrypted_payload = "payload",
                   .encapped_key = "key",
                   .signature = "signature",
               })
                  .BuildEncStructureForEncrypting("aad"),
              IsOkAndHolds(absl::string_view(
                  "\x83"             // array with 3 items:
                  "\x68\x45ncrypt0"  // "Encrypt0"
                  "\x58\x21\xa3"     // bstr containing a map with 3 items:
                  "\x01\x02"         // alg: 2
                  "\x3a\x00\x01\x00\x01\x49old-state"  // -65538: b"old-state"
                  "\x3a\x00\x01\x00\x02\x49new-state"  // -65539: b"new-state"
                  "\x43\x61\x61\x64",                  // b"aad"
                  49)));
}

TEST(ReleaseTokenTest, GetEncStructureForDecrypting) {
  EXPECT_THAT(ReleaseToken::GetEncStructureForDecrypting(
                  absl::string_view(
                      "\x84\x43\xa1\x01\x01\xa0\x58\x3e\x83\x58\x21\xa3\x01\x02"
                      "\x3a\x00\x01\x00\x01\x49old-state\x3a\x00\x01\x00\x02"
                      "\x49new-state\xa2\x04\x46key-id\x3a\x00\x01\x00\x00\x43"
                      "key\x47payload\x49signature",
                      80),
                  "aad"),
              IsOkAndHolds(absl::string_view(
                  "\x83"             // array with 3 items:
                  "\x68\x45ncrypt0"  // "Encrypt0"
                  "\x58\x21\xa3"     // bstr containing a map with 3 items:
                  "\x01\x02"         // alg: 2
                  "\x3a\x00\x01\x00\x01\x49old-state"  // -65538: b"old-state"
                  "\x3a\x00\x01\x00\x02\x49new-state"  // -65539: b"new-state"
                  "\x43\x61\x61\x64",                  // b"aad"
                  49)));
}

TEST(ReleaseTokenTest, BuildSigStructureForSigning) {
  EXPECT_THAT((ReleaseToken{
                   .signing_algorithm = 1,
                   .encryption_algorithm = 2,
                   .encryption_key_id = "key-id",
                   .src_state = "old-state",
                   .dst_state = "new-state",
                   .encrypted_payload = "payload",
                   .encapped_key = "key",
                   .signature = "signature",
               })
                  .BuildSigStructureForSigning("aad"),
              IsOkAndHolds(absl::string_view(
                  "\x84"              // array with 4 items:
                  "\x6aSignature1"    // "Signature1"
                  "\x43\xa1\x01\x01"  // bstr containing { alg: 1 }
                  "\x43\x61\x61\x64"  // b"aad"
                  // bstr containing COSE_Encrypt0
                  "\x58\x3e\x83\x58\x21\xa3\x01\x02\x3a\x00\x01\x00\x01\x49old-"
                  "state\x3a\x00\x01\x00\x02\x49new-state\xa2\x04\x46key-"
                  "id\x3a\x00\x01\x00\x00\x43key\x47payload",
                  84)));
}

TEST(ReleaseTokenTest, GetSigStructureForVerifying) {
  EXPECT_THAT(ReleaseToken::GetSigStructureForVerifying(
                  absl::string_view(
                      "\x84\x43\xa1\x01\x01\xa0\x58\x3e\x83\x58\x21\xa3\x01\x02"
                      "\x3a\x00\x01\x00\x01\x49old-state\x3a\x00\x01\x00\x02"
                      "\x49new-state\xa2\x04\x46key-id\x3a\x00\x01\x00\x00\x43"
                      "key\x47payload\x49signature",
                      80),
                  "aad"),
              IsOkAndHolds(absl::string_view(
                  "\x84"              // array with 4 items:
                  "\x6aSignature1"    // "Signature1"
                  "\x43\xa1\x01\x01"  // bstr containing { alg: 1 }
                  "\x43\x61\x61\x64"  // b"aad"
                  // bstr containing COSE_Encrypt0
                  "\x58\x3e\x83\x58\x21\xa3\x01\x02\x3a\x00\x01\x00\x01\x49old-"
                  "state\x3a\x00\x01\x00\x02\x49new-state\xa2\x04\x46key-"
                  "id\x3a\x00\x01\x00\x00\x43key\x47payload",
                  84)));
}

}  // namespace
}  // namespace fcp::confidential_compute
