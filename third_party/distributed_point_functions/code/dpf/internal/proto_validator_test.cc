// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/internal/proto_validator.h"

#include <stdint.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/internal/proto_validator_test_textproto_embed.h"
#include "dpf/internal/status_matchers.h"
#include "dpf/tuple.h"
#include "gmock/gmock.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace dpf_internal {
namespace {

using ::testing::Ne;
using ::testing::StartsWith;

class ProtoValidatorTest : public testing::Test {
 protected:
  void SetUp() override {
    const auto* const toc = proto_validator_test_textproto_embed_create();
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        std::string(toc->data, toc->size), &ctx_));
    parameters_ = std::vector<DpfParameters>(ctx_.parameters().begin(),
                                             ctx_.parameters().end());
    dpf_key_ = ctx_.key();
    DPF_ASSERT_OK_AND_ASSIGN(proto_validator_,
                             ProtoValidator::Create(parameters_));
  }

  std::vector<DpfParameters> parameters_;
  DpfKey dpf_key_;
  EvaluationContext ctx_;
  std::unique_ptr<dpf_internal::ProtoValidator> proto_validator_;
};

TEST_F(ProtoValidatorTest, CreateFailsWithoutParameters) {
  EXPECT_THAT(ProtoValidator::Create({}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`parameters` must not be empty"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenParametersNotSorted) {
  parameters_.resize(2);
  parameters_[0].set_log_domain_size(10);
  parameters_[1].set_log_domain_size(8);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`log_domain_size` fields must be in ascending order in "
                       "`parameters`"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenDomainSizeNegative) {
  parameters_.resize(1);
  parameters_[0].set_log_domain_size(-1);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`log_domain_size` must be non-negative"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenDomainSizeTooLarge) {
  parameters_.resize(1);
  parameters_[0].set_log_domain_size(129);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`log_domain_size` must be <= 128"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenElementBitsizeNegative) {
  parameters_.resize(1);
  parameters_[0].mutable_value_type()->mutable_integer()->set_bitsize(-1);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be positive"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenElementBitsizeZero) {
  parameters_.resize(1);
  parameters_[0].mutable_value_type()->mutable_integer()->set_bitsize(0);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be positive"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenElementBitsizeTooLarge) {
  parameters_.resize(1);
  parameters_[0].mutable_value_type()->mutable_integer()->set_bitsize(256);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be less than or equal to 128"));
}

TEST_F(ProtoValidatorTest, CreateFailsWhenElementBitsizeNotAPowerOfTwo) {
  parameters_.resize(1);
  parameters_[0].mutable_value_type()->mutable_integer()->set_bitsize(23);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be a power of 2"));
}

TEST_F(ProtoValidatorTest, CreateFailsIfSecurityParameterIsNaN) {
  parameters_.resize(1);
  parameters_[0].set_security_parameter(std::nan(""));

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`security_parameter` must not be NaN"));
}

TEST_F(ProtoValidatorTest, CreateFailsIfSecurityParameterIsNegative) {
  parameters_.resize(1);
  parameters_[0].set_security_parameter(-0.01);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`security_parameter` must be in [0, 128]"));
}

TEST_F(ProtoValidatorTest, CreateFailsIfSecurityParameterIsTooLarge) {
  parameters_.resize(1);
  parameters_[0].set_security_parameter(128.01);

  EXPECT_THAT(ProtoValidator::Create(parameters_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`security_parameter` must be in [0, 128]"));
}

TEST_F(ProtoValidatorTest, CreateWorksWhenElementBitsizesDecrease) {
  parameters_.resize(2);
  parameters_[0].mutable_value_type()->mutable_integer()->set_bitsize(64);
  parameters_[1].mutable_value_type()->mutable_integer()->set_bitsize(32);

  EXPECT_THAT(ProtoValidator::Create(parameters_), IsOkAndHolds(Ne(nullptr)));
}

TEST_F(ProtoValidatorTest, CreateWorksWhenHierarchiesAreFarApart) {
  parameters_.resize(2);
  parameters_[0].set_log_domain_size(10);
  parameters_[1].set_log_domain_size(128);

  EXPECT_THAT(ProtoValidator::Create(parameters_), IsOkAndHolds(Ne(nullptr)));
}

TEST_F(ProtoValidatorTest,
       ValidateDpfKeyFailsIfNumberOfCorrectionWordsDoesntMatch) {
  dpf_key_.add_correction_words();

  EXPECT_THAT(proto_validator_->ValidateDpfKey(dpf_key_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       absl::StrCat("Malformed DpfKey: expected ",
                                    dpf_key_.correction_words_size() - 1,
                                    " correction words, but got ",
                                    dpf_key_.correction_words_size())));
}

TEST_F(ProtoValidatorTest, ValidateDpfKeyFailsIfSeedIsMissing) {
  dpf_key_.clear_seed();

  EXPECT_THAT(
      proto_validator_->ValidateDpfKey(dpf_key_),
      StatusIs(absl::StatusCode::kInvalidArgument, "key.seed must be present"));
}

TEST_F(ProtoValidatorTest,
       ValidateDpfKeyFailsIfLastLevelOutputCorrectionIsMissing) {
  dpf_key_.clear_last_level_value_correction();

  EXPECT_THAT(proto_validator_->ValidateDpfKey(dpf_key_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "key.last_level_value_correction must be present"));
}

TEST_F(ProtoValidatorTest, ValidateDpfKeyFailsIfOutputCorrectionIsMissing) {
  for (CorrectionWord& cw : *(dpf_key_.mutable_correction_words())) {
    cw.clear_value_correction();
  }

  EXPECT_THAT(
      proto_validator_->ValidateDpfKey(dpf_key_),
      StatusIs(absl::StatusCode::kInvalidArgument,
               StartsWith("Malformed DpfKey: expected correction_words")));
}

TEST_F(ProtoValidatorTest, ValidateEvaluationContextFailsIfKeyIsMissing) {
  ctx_.clear_key();

  EXPECT_THAT(
      proto_validator_->ValidateEvaluationContext(ctx_),
      StatusIs(absl::StatusCode::kInvalidArgument, "ctx.key must be present"));
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextFailsIfParameterSizeDoesntMatch) {
  ctx_.mutable_parameters()->erase(ctx_.parameters().end() - 1);

  EXPECT_THAT(proto_validator_->ValidateEvaluationContext(ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Number of parameters in `ctx` doesn't match"));
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextFailsIfLogDomainSizeDoesntMatch) {
  ctx_.mutable_parameters(0)->set_log_domain_size(
      ctx_.parameters(0).log_domain_size() + 1);

  EXPECT_THAT(proto_validator_->ValidateEvaluationContext(ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Parameter 0 in `ctx` doesn't match"));
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextSucceedsIfSecurityParameterIsDefault) {
  parameters_[0].set_security_parameter(0);
  DPF_ASSERT_OK_AND_ASSIGN(proto_validator_,
                           ProtoValidator::Create(parameters_));

  ctx_.mutable_parameters(0)->set_security_parameter(0);

  EXPECT_THAT(proto_validator_->ValidateEvaluationContext(ctx_), IsOk());
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextFailsIfSecurityParameterDoesntMatch) {
  ctx_.mutable_parameters(0)->set_security_parameter(
      ctx_.parameters(0).security_parameter() + 1);

  EXPECT_THAT(proto_validator_->ValidateEvaluationContext(ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Parameter 0 in `ctx` doesn't match"));
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextFailsIfContextFullyEvaluated) {
  ctx_.set_previous_hierarchy_level(parameters_.size() - 1);

  EXPECT_THAT(proto_validator_->ValidateEvaluationContext(ctx_),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "This context has already been fully evaluated"));
}

TEST_F(ProtoValidatorTest,
       ValidateEvaluationContextFailsIfPartialEvaluationsLevelTooLarge) {
  ctx_.set_previous_hierarchy_level(0);
  ctx_.set_partial_evaluations_level(1);
  ctx_.add_partial_evaluations();

  EXPECT_THAT(
      proto_validator_->ValidateEvaluationContext(ctx_),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "ctx.partial_evaluations_level must be less than or equal to "
               "ctx.previous_hierarchy_level"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfTypeNotInteger) {
  ValueType type;
  type.mutable_integer()->set_bitsize(32);
  Value value;
  value.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(
      23);

  EXPECT_THAT(
      proto_validator_->ValidateValue(value, type),
      StatusIs(absl::StatusCode::kInvalidArgument, "Expected integer value"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfIntegerTooLarge) {
  ValueType type;
  Value value;

  int element_bitsize = 32;
  type.mutable_integer()->set_bitsize(element_bitsize);
  auto value_64 = uint64_t{1} << element_bitsize;
  value.mutable_integer()->set_value_uint64(value_64);

  EXPECT_THAT(
      proto_validator_->ValidateValue(value, type),
      StatusIs(absl::StatusCode::kInvalidArgument,
               absl::StrFormat(
                   "Value (= %d) too large for ValueType with bitsize = %d",
                   value_64, element_bitsize)));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfTypeNotTuple) {
  ValueType type;
  type.mutable_tuple()->add_elements()->mutable_integer()->set_bitsize(32);
  Value value;
  value.mutable_integer()->set_value_uint64(23);

  EXPECT_THAT(
      proto_validator_->ValidateValue(value, type),
      StatusIs(absl::StatusCode::kInvalidArgument, "Expected tuple value"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfTupleSizeDoesntMatch) {
  ValueType type;
  type.mutable_tuple()->add_elements()->mutable_integer()->set_bitsize(32);
  Value value;

  value.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(
      23);
  value.mutable_tuple()->add_elements()->mutable_integer()->set_value_uint64(
      42);

  EXPECT_THAT(proto_validator_->ValidateValue(value, type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Expected tuple value of size 1 but got size 2"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfValueLargerThanModulus) {
  constexpr uint64_t kModulus = 3;
  ValueType type;
  type.mutable_int_mod_n()->mutable_base_integer()->set_bitsize(64);
  type.mutable_int_mod_n()->mutable_modulus()->set_value_uint64(kModulus);
  Value value;

  value.mutable_int_mod_n()->set_value_uint64(kModulus);

  EXPECT_THAT(proto_validator_->ValidateValue(value, type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Value (= 3) is too large for modulus (= 3)"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfTypeNotXorWrapper) {
  ValueType type;
  type.mutable_xor_wrapper()->set_bitsize(32);
  Value value;
  value.mutable_integer()->set_value_uint64(23);

  EXPECT_THAT(proto_validator_->ValidateValue(value, type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Expected XorWrapper value"));
}

TEST_F(ProtoValidatorTest, ValidateValueFailsIfValueIsUnknown) {
  ValueType type;
  Value value;

  EXPECT_THAT(
      proto_validator_->ValidateValue(value, type),
      StatusIs(absl::StatusCode::kInvalidArgument,
               testing::StartsWith("ValidateValue: Unsupported ValueType:")));
}

TEST(ProtoValidator, ValidateValueTypeFailsIfBitsizeNotPositive) {
  ValueType type;

  type.mutable_integer()->set_bitsize(0);

  EXPECT_THAT(ProtoValidator::ValidateValueType(type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be positive"));
}

TEST(ProtoValidator, ValidateValueTypeFailsIfBitsizeTooLarge) {
  ValueType type;

  type.mutable_integer()->set_bitsize(256);

  EXPECT_THAT(ProtoValidator::ValidateValueType(type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be less than or equal to 128"));
}

TEST(ProtoValidator, ValidateValueTypeFailsIfBitsizeNotPowerOfTwo) {
  ValueType type;

  type.mutable_integer()->set_bitsize(17);

  EXPECT_THAT(ProtoValidator::ValidateValueType(type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "`bitsize` must be a power of 2"));
}

TEST(ProtoValidator, ValidateValueTypeFailsIfNoTypeChosen) {
  ValueType type;

  EXPECT_THAT(ProtoValidator::ValidateValueType(type),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       StartsWith("ValidateValueType: Unsupported ValueType")));
}

}  // namespace
}  // namespace dpf_internal
}  // namespace distributed_point_functions
