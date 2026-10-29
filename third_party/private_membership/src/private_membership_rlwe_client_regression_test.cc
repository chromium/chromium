#include <fstream>
#include <sstream>
#include <string>

#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "third_party/private-join-and-compute/src/private_join_and_compute/crypto/context.h"
#include "third_party/private_membership/src/internal/rlwe_id_utils.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace rlwe {
namespace {

using ::testing::Eq;

constexpr char kTestDataPath[] =
  "third_party/private_membership/src/internal/testing/regression_test_data/";

absl::StatusOr<std::string> ReadFileToString(absl::string_view path) {
  std::ifstream file((std::string(path)));

  if (!file.is_open()) {
    return absl::InternalError("Reading file failed.");
  }

  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

template <class T>
absl::Status ParseProtoFromFile(absl::string_view path, T* proto_out) {
  absl::StatusOr<std::string> serialized_proto = ReadFileToString(path);
  if (!serialized_proto.ok()) {
    return serialized_proto.status();
  }
  if (!proto_out->ParseFromString(*serialized_proto)) {
    return absl::InternalError("Proto parsing failed.");
  }
  return absl::OkStatus();
}

void VerifyClient(
    const PrivateMembershipRlweClientRegressionTestData::TestCase& test_case) {
  ASSERT_OK_AND_ASSIGN(auto client,
                       PrivateMembershipRlweClient::CreateForTesting(
                           test_case.use_case(), {test_case.plaintext_id()},
                           test_case.ec_cipher_key(), test_case.seed()));

  ASSERT_OK_AND_ASSIGN(auto oprf_request, client->CreateOprfRequest());
  EXPECT_EQ(oprf_request.SerializeAsString(),
            test_case.expected_oprf_request().SerializeAsString());

  ASSERT_OK_AND_ASSIGN(auto query_request,
                       client->CreateQueryRequest(test_case.oprf_response()));
  EXPECT_EQ(query_request.SerializeAsString(),
            test_case.expected_query_request().SerializeAsString());

  ASSERT_OK_AND_ASSIGN(
      auto membership_response_proto,
      client->ProcessQueryResponse(test_case.query_response()));
  EXPECT_THAT(membership_response_proto.membership_responses_size(), Eq(1));
  EXPECT_THAT(membership_response_proto.membership_responses(0)
                  .plaintext_id()
                  .SerializeAsString(),
              Eq(test_case.plaintext_id().SerializeAsString()));
  EXPECT_THAT(membership_response_proto.membership_responses(0)
                  .membership_response()
                  .is_member(),
              Eq(test_case.is_positive_membership_expected()));
}

TEST(PrivateMembershipRlweClientRegressionTest, TestMembership) {
  PrivateMembershipRlweClientRegressionTestData test_data;
  EXPECT_OK(ParseProtoFromFile(
      absl::StrCat(kTestDataPath, "test_data.binarypb"), &test_data));

  EXPECT_THAT(test_data.test_cases_size(), Eq(10));
  for (const auto& test_case : test_data.test_cases()) {
    VerifyClient(test_case);
  }
}

}  // namespace

class PrivateMembershipRlweClientTestPeer {
 public:
  static ::rlwe::StatusOr<private_membership::MembershipResponse> CheckMembership(
      PrivateMembershipRlweClient* client,
      absl::string_view server_encrypted_id,
      const private_membership::rlwe::EncryptedBucket& encrypted_bucket) {
    return client->CheckMembership(server_encrypted_id, encrypted_bucket);
  }

  static void SetEncryptedBucketParams(
      PrivateMembershipRlweClient* client,
      const private_membership::rlwe::EncryptedBucketsParameters& params) {
    client->encrypted_bucket_params_ = params;
  }

  static ::private_join_and_compute::Context* GetContext(
      PrivateMembershipRlweClient* client) {
    return &client->context_;
  }
};

namespace {

TEST(PrivateMembershipRlweClientTest, CheckMembershipOversizedEncryptedId) {
  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid");
  plaintext_id.set_sensitive_id("sid");
  ASSERT_OK_AND_ASSIGN(
      auto client,
      PrivateMembershipRlweClient::Create(
          private_membership::rlwe::TEST_USE_CASE,
          {plaintext_id}));

  private_membership::rlwe::EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(8);
  params.set_sensitive_id_hash_type(private_membership::SHA256_NON_SENSITIVE_AND_SENSITIVE_ID);
  PrivateMembershipRlweClientTestPeer::SetEncryptedBucketParams(client.get(), params);

  std::string server_encrypted_id = "some_server_encrypted_id";
  ASSERT_OK_AND_ASSIGN(
      std::string to_match_hash,
      ComputeBucketStoredEncryptedId(server_encrypted_id, params,
                                     PrivateMembershipRlweClientTestPeer::GetContext(client.get())));

  // Craft an oversized encrypted_id starting with `to_match_hash`.
  std::string oversized_encrypted_id = to_match_hash + "extra_bytes_that_make_it_longer";

  // Create an EncryptedBucket containing this oversized encrypted_id.
  private_membership::rlwe::EncryptedBucket encrypted_bucket;
  auto* pair = encrypted_bucket.add_encrypted_id_value_pairs();
  pair->set_encrypted_id(oversized_encrypted_id);
  pair->set_encrypted_value("some_encrypted_value");

  // Call CheckMembership.
  ASSERT_OK_AND_ASSIGN(
      auto response,
      PrivateMembershipRlweClientTestPeer::CheckMembership(
          client.get(), server_encrypted_id, encrypted_bucket));

  // The oversized encrypted_id should not be considered a match, because its size is greater
  // than `to_match_hash.size()`.
  EXPECT_FALSE(response.is_member());
}

TEST(PrivateMembershipRlweClientTest, CheckMembershipExactMatch) {
  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid");
  plaintext_id.set_sensitive_id("sid");
  ASSERT_OK_AND_ASSIGN(
      auto client,
      PrivateMembershipRlweClient::Create(
          private_membership::rlwe::TEST_USE_CASE,
          {plaintext_id}));

  private_membership::rlwe::EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(8);
  params.set_sensitive_id_hash_type(private_membership::SHA256_NON_SENSITIVE_AND_SENSITIVE_ID);
  PrivateMembershipRlweClientTestPeer::SetEncryptedBucketParams(client.get(), params);

  std::string server_encrypted_id = "some_server_encrypted_id";
  ASSERT_OK_AND_ASSIGN(
      std::string to_match_hash,
      ComputeBucketStoredEncryptedId(server_encrypted_id, params,
                                     PrivateMembershipRlweClientTestPeer::GetContext(client.get())));

  // Exact matching encrypted_id.
  std::string matching_encrypted_id = to_match_hash;

  // Create an EncryptedBucket containing this matching encrypted_id.
  private_membership::rlwe::EncryptedBucket encrypted_bucket;
  auto* pair = encrypted_bucket.add_encrypted_id_value_pairs();
  pair->set_encrypted_id(matching_encrypted_id);

  // Call CheckMembership.
  ASSERT_OK_AND_ASSIGN(
      auto response,
      PrivateMembershipRlweClientTestPeer::CheckMembership(
          client.get(), server_encrypted_id, encrypted_bucket));

  // It should be a member.
  EXPECT_TRUE(response.is_member());
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership

