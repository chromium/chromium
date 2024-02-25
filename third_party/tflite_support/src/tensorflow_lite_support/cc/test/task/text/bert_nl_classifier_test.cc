/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"

#include <fcntl.h>

#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace text {

namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::Category;
using ::tflite::task::core::LoadBinaryContent;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "text/";

constexpr char kTestModelPath[] = "bert_nl_classifier.tflite";

constexpr char kInvalidModelPath[] = "i/do/not/exist.tflite";

constexpr int kMaxSeqLen = 128;

std::string GetFullPath(absl::string_view file_name) {
  return JoinPath("./" /*test src dir*/, kTestDataDirectory,
                  file_name);
}

class BertNLClassifierTest : public tflite::testing::Test {};

TEST_F(BertNLClassifierTest, CreateFromOptionsSucceedsWithModelWithMetadata) {
  BertNLClassifierOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelPath));

  SUPPORT_ASSERT_OK(BertNLClassifier::CreateFromOptions(options));
}

TEST_F(BertNLClassifierTest, CreateFromOptionsFailsWithMissingBaseOptions) {
  BertNLClassifierOptions options;
  StatusOr<std::unique_ptr<BertNLClassifier>> classifier_or =
      BertNLClassifier::CreateFromOptions(options);

  EXPECT_EQ(classifier_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(classifier_or.status().message(),
              HasSubstr("Missing mandatory `base_options`"));
  EXPECT_THAT(classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(BertNLClassifierTest, TestNLClassifierCreationFilePath) {
  SUPPORT_ASSERT_OK(BertNLClassifier::CreateFromFile(GetFullPath(kTestModelPath)));
}

TEST_F(BertNLClassifierTest, TestNLClassifierCreationBinary) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestModelPath).c_str());
  SUPPORT_ASSERT_OK(BertNLClassifier::CreateFromBuffer(model_buffer.data(),
                                               model_buffer.size()));
}

TEST_F(BertNLClassifierTest, TestNLClassifierCreationFailure) {
  StatusOr<std::unique_ptr<BertNLClassifier>> classifier_or =
      BertNLClassifier::CreateFromFile(kInvalidModelPath);

  EXPECT_EQ(classifier_or.status().code(), absl::StatusCode::kNotFound);
  EXPECT_THAT(classifier_or.status().message(),
              HasSubstr("Unable to open file at i/do/not/exist.tflite"));
  EXPECT_THAT(classifier_or.status().GetPayload(kTfLiteSupportPayload),
              ::testing::Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kFileNotFoundError))));
}

Category* GetCategoryWithClassName(const std::string& class_name,
                                   std::vector<Category>& categories) {
  for (Category& category : categories) {
    if (category.class_name == class_name) {
      return &category;
    }
  }
  return nullptr;
}

void verify_classifier(std::unique_ptr<BertNLClassifier> classifier,
                       bool verify_positive) {
  if (verify_positive) {
    std::vector<core::Category> results =
        classifier->Classify("unflinchingly bleak and desperate");
    EXPECT_GT(GetCategoryWithClassName("negative", results)->score,
              GetCategoryWithClassName("positive", results)->score);
  } else {
    std::vector<Category> results =
        classifier->Classify("it's a charming and often affecting journey");
    EXPECT_GT(GetCategoryWithClassName("positive", results)->score,
              GetCategoryWithClassName("negative", results)->score);
  }
}

TEST_F(BertNLClassifierTest, ClassifySucceedsWithBaseOptions) {
  std::unique_ptr<BertNLClassifier> classifier;

  // Test creating BertNLClassifier when classifier outlives options.
  {
    std::string contents =
        LoadBinaryContent(GetFullPath(kTestModelPath).c_str());
    BertNLClassifierOptions options;
    options.mutable_base_options()->mutable_model_file()->set_file_content(
        contents);

    SUPPORT_ASSERT_OK_AND_ASSIGN(classifier,
                         BertNLClassifier::CreateFromOptions(options));
  }

  verify_classifier(std::move(classifier), /*verify_positive=*/false);
}

TEST_F(BertNLClassifierTest, TestNLClassifier_ClassifyNegative) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestModelPath).c_str());
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<BertNLClassifier> classifier,
                       BertNLClassifier::CreateFromBuffer(model_buffer.data(),
                                                          model_buffer.size()));

  verify_classifier(std::move(classifier), false);
}

TEST_F(BertNLClassifierTest, TestNLClassifier_ClassifyPositive) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestModelPath).c_str());
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<BertNLClassifier> classifier,
                       BertNLClassifier::CreateFromBuffer(model_buffer.data(),
                                                          model_buffer.size()));

  verify_classifier(std::move(classifier), true);
}

TEST_F(BertNLClassifierTest, TestNLClassifierFd_ClassifyPositive) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<BertNLClassifier> classifier,
                       BertNLClassifier::CreateFromFd(open(
                           GetFullPath(kTestModelPath).c_str(), O_RDONLY)));

  verify_classifier(std::move(classifier), false);
}

TEST_F(BertNLClassifierTest, TestNLClassifierFd_ClassifyNegative) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<BertNLClassifier> classifier,
                       BertNLClassifier::CreateFromFd(open(
                           GetFullPath(kTestModelPath).c_str(), O_RDONLY)));

  verify_classifier(std::move(classifier), true);
}

// BertNLClassifier limits the input sequence to kMaxSeqLen, test when input is
// longer than this the classifier still works correctly.
TEST_F(BertNLClassifierTest, TestNLClassifier_ClassifyLongPositive_notOOB) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestModelPath).c_str());
  std::stringstream ss_for_positive_review;
  ss_for_positive_review
      << "it's a charming and often affecting journey and this is a long";
  for (int i = 0; i < kMaxSeqLen; ++i) {
    ss_for_positive_review << " long";
  }
  ss_for_positive_review << " movie review";
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<BertNLClassifier> classifier,
                       BertNLClassifier::CreateFromBuffer(model_buffer.data(),
                                                          model_buffer.size()));

  std::vector<core::Category> results =
      classifier->Classify(ss_for_positive_review.str());

  EXPECT_GT(GetCategoryWithClassName("positive", results)->score,
            GetCategoryWithClassName("negative", results)->score);
}

}  // namespace

}  // namespace text
}  // namespace task
}  // namespace tflite
