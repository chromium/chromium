#include "base/files/file_util.h"
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

absl::Status ReadFileToString(absl::string_view path, std::string* str_out) {
  if (base::ReadFileToString(base::FilePath(path.data()), str_out)) {
    return absl::OkStatus();
  }
  return absl::InternalError("Reading file failed.");
}

template <class T>
absl::Status ParseProtoFromFile(absl::string_view path, T* proto_out) {
  std::string serialized_proto;
  auto status = ReadFileToString(path, &serialized_proto);
  if (!status.ok()) {
    return status;
  }
  if (!proto_out->ParseFromString(serialized_proto)) {
    return absl::InternalError("Proto parsing failed.");
  }
  return absl::OkStatus();
}

void VerifyClient(
    const PrivateMembershipRlweClientRegressionTestData::TestCase& test_case) {
  auto client_or_status = PrivateMembershipRlweClient::CreateForTesting(
      test_case.use_case(), {test_case.plaintext_id()},
      test_case.ec_cipher_key(), test_case.seed());
  EXPECT_OK(client_or_status.status());
  auto client = std::move(client_or_status.value());

  ASSERT_OK_AND_ASSIGN(auto oprf_request, client->CreateOprfRequest());
  EXPECT_EQ(oprf_request.SerializeAsString(),
            test_case.expected_oprf_request().SerializeAsString());

  ASSERT_OK_AND_ASSIGN(auto query_request,
                       client->CreateQueryRequest(test_case.oprf_response()));
  EXPECT_EQ(query_request.SerializeAsString(),
            test_case.expected_query_request().SerializeAsString());

  ASSERT_OK_AND_ASSIGN(auto membership_response_map,
                       client->ProcessResponse(test_case.query_response()));
  EXPECT_THAT(membership_response_map.Get(test_case.plaintext_id()).is_member(),
              Eq(test_case.is_positive_membership_expected()));
}

TEST(PrivateMembershipRlweClientRegressionTest, TestMembershipCros) {
  PrivateMembershipRlweClientRegressionTestData test_data;
  EXPECT_OK(ParseProtoFromFile(
      absl::StrCat(kTestDataPath, "cros_test_data.binarypb"), &test_data));

  EXPECT_THAT(test_data.test_cases_size(), Eq(10));
  for (const auto& test_case : test_data.test_cases()) {
    VerifyClient(test_case);
  }
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership
