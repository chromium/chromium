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

#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"

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
using ::tflite::task::JoinPath;
using ::tflite::task::core::LoadBinaryContent;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "text/";

constexpr char kTestMobileBertModelPath[] = "mobilebert_float.tflite";
constexpr char kTestVocabPath[] = "mobilebert_vocab.txt";
constexpr char kTestMobileBertWithMetadataModelPath[] =
    "mobilebert_with_metadata.tflite";
constexpr char kTestAlBertModelPath[] = "albert.tflite";
constexpr char kTestSPModelPath[] = "30k-clean.model";
constexpr char kTestAlbertWithMetadataModelPath[] =
    "albert_with_metadata.tflite";

constexpr char kQuestion[] = "What is a course of study called?";
constexpr char kAnswer[] = "the curriculum.";
constexpr char kContext[] =
    "The role of teacher is often formal and ongoing, carried out at a school "
    "or other place of formal education. In many countries, a person who "
    "wishes to become a teacher must first obtain specified professional "
    "qualifications or credentials from a university or college. These "
    "professional qualifications may include the study of pedagogy, the "
    "science of teaching. Teachers, like other professionals, may have to "
    "continue their education after they qualify, a process known as "
    "continuing professional development. Teachers may use a lesson plan to "
    "facilitate student learning, providing a course of study which is called "
    "the curriculum.";
constexpr int kPredictAnsNum = 5;

class BertQuestionAnswererTest : public tflite::testing::Test {};

std::string GetFullPath(absl::string_view file_name) {
  return JoinPath("./" /*test src dir*/, kTestDataDirectory,
                  file_name);
}

TEST_F(BertQuestionAnswererTest,
       CreateFromOptionsSucceedsWithModelWithMetadata) {
  BertQuestionAnswererOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestMobileBertWithMetadataModelPath));

  SUPPORT_ASSERT_OK(BertQuestionAnswerer::CreateFromOptions(options));
}

TEST_F(BertQuestionAnswererTest, CreateFromOptionsFailsWithMissingBaseOptions) {
  BertQuestionAnswererOptions options;
  StatusOr<std::unique_ptr<QuestionAnswerer>> question_answerer_or =
      BertQuestionAnswerer::CreateFromOptions(options);

  EXPECT_EQ(question_answerer_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(question_answerer_or.status().message(),
              HasSubstr("Missing mandatory `base_options`"));
  EXPECT_THAT(question_answerer_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(BertQuestionAnswererTest, AnswerSucceedsWithModelWithMetadata) {
  std::unique_ptr<QuestionAnswerer> question_answerer;

  // Test creating NLClassifier when question_answerer outlives options.
  {
    std::string contents = LoadBinaryContent(
        GetFullPath(kTestMobileBertWithMetadataModelPath).c_str());

    BertQuestionAnswererOptions options;
    options.mutable_base_options()->mutable_model_file()->set_file_content(
        contents);

    SUPPORT_ASSERT_OK_AND_ASSIGN(question_answerer,
                         BertQuestionAnswerer::CreateFromOptions(options));
  }

  std::vector<QaAnswer> answer = question_answerer->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest, TestBertCreationFromBinary) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestMobileBertModelPath).c_str());
  std::string vocab_buffer =
      LoadBinaryContent(GetFullPath(kTestVocabPath).c_str());
  SUPPORT_ASSERT_OK(BertQuestionAnswerer::CreateBertQuestionAnswererFromBuffer(
      model_buffer.data(), model_buffer.size(), vocab_buffer.data(),
      vocab_buffer.size()));
}

TEST_F(BertQuestionAnswererTest, TestBertCreationFromFile) {
  SUPPORT_ASSERT_OK(BertQuestionAnswerer::CreateBertQuestionAnswererFromFile(
      GetFullPath(kTestMobileBertModelPath).c_str(),
      GetFullPath(kTestVocabPath).c_str()));
}

TEST_F(BertQuestionAnswererTest, TestBertAnswer) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<QuestionAnswerer> question_answerer_status,
      BertQuestionAnswerer::CreateBertQuestionAnswererFromFile(
          GetFullPath(kTestMobileBertModelPath).c_str(),
          GetFullPath(kTestVocabPath).c_str()));

  std::vector<QaAnswer> answer =
      question_answerer_status->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest, TestAlbertCreationFromBinary) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestAlBertModelPath).c_str());
  std::string vocab_buffer =
      LoadBinaryContent(GetFullPath(kTestSPModelPath).c_str());
  SUPPORT_ASSERT_OK(BertQuestionAnswerer::CreateAlbertQuestionAnswererFromBuffer(
      model_buffer.data(), model_buffer.size(), vocab_buffer.data(),
      vocab_buffer.size()));
}

TEST_F(BertQuestionAnswererTest, TestAlbertCreationFromFile) {
  SUPPORT_ASSERT_OK(BertQuestionAnswerer::CreateAlbertQuestionAnswererFromFile(
      GetFullPath(kTestAlBertModelPath).c_str(),
      GetFullPath(kTestSPModelPath).c_str()));
}

TEST_F(BertQuestionAnswererTest, TestAlbertAnswer) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<QuestionAnswerer> question_answerer_status,
      BertQuestionAnswerer::CreateAlbertQuestionAnswererFromFile(
          GetFullPath(kTestAlBertModelPath).c_str(),
          GetFullPath(kTestSPModelPath).c_str()));

  std::vector<QaAnswer> answer =
      question_answerer_status->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest, TestCreateWithMetadata) {
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<QuestionAnswerer> question_answerer_status,
      BertQuestionAnswerer::CreateFromFile(
          GetFullPath(kTestMobileBertWithMetadataModelPath).c_str()));

  std::vector<QaAnswer> answer =
      question_answerer_status->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest, TestCreateWithMetadataFromBinary) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestAlbertWithMetadataModelPath).c_str());

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<QuestionAnswerer> question_answerer_status,
      BertQuestionAnswerer::CreateFromBuffer(model_buffer.data(),
                                             model_buffer.size()));

  std::vector<QaAnswer> answer =
      question_answerer_status->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest, TestCreateWithFileDescriptor2) {
  std::string model_buffer =
      LoadBinaryContent(GetFullPath(kTestAlbertWithMetadataModelPath).c_str());

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<QuestionAnswerer> question_answerer_status,
      BertQuestionAnswerer::CreateFromFd(open(
          GetFullPath(kTestAlbertWithMetadataModelPath).c_str(), O_RDONLY)));

  std::vector<QaAnswer> answer =
      question_answerer_status->Answer(kContext, kQuestion);
  ASSERT_EQ(answer.size(), kPredictAnsNum);
  EXPECT_EQ(answer[0].text, kAnswer);
}

TEST_F(BertQuestionAnswererTest,
       TestCreateWithMetadataFail_fromModelWithoutMetadata) {
  StatusOr<std::unique_ptr<QuestionAnswerer>> question_answerer_or =
      BertQuestionAnswerer::CreateFromFile(
          GetFullPath(kTestMobileBertModelPath).c_str());

  EXPECT_EQ(question_answerer_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(question_answerer_or.status().message(),
              HasSubstr("No input process unit found from metadata."));
  EXPECT_THAT(question_answerer_or.status().GetPayload(
                  tflite::support::kTfLiteSupportPayload),
              ::testing::Optional(absl::Cord(absl::StrCat(
                  TfLiteSupportStatus::kMetadataInvalidTokenizerError))));
}

}  // namespace
}  // namespace text
}  // namespace task
}  // namespace tflite
