#include <fstream>
#include <sstream>
#include <string>

#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
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
}  // namespace rlwe
}  // namespace private_membership
