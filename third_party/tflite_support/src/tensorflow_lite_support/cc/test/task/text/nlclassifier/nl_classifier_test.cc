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

#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

#include <utility>

#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/core/base_task_api.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/test/task/text/nlclassifier/nl_classifier_test_utils.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

static constexpr char kInputStr[] = "hello";

namespace tflite {
namespace task {
namespace text {
namespace nlclassifier {
namespace {

using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAreArray;
using ::testing::ValuesIn;
using ::tflite::support::kTfLiteSupportPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::JoinPath;
using ::tflite::task::core::LoadBinaryContent;
using NLClassifierProtoOptions = ::tflite::task::text::NLClassifierOptions;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "text/";

// The model has 1 input tensor and 4 output tensors with the following names
// and indices.
// The model also has three custom OPs to mimic classification model behaviors,
// see CUSTOM_OP_STRING_TO_FLOATS, CUSTOM_OP_STRING_TO_DOUBLES
// and CUSTOM_OP_GENERATE_LABELS in nl_classifier_test_utils for details.
constexpr char kTestModelPath[] = "test_model_nl_classifier.tflite";

// The model has 1 input tensor and 1 output tensor
// The model also has a custom OP to mimic classification model behaviors,
// see CUSTOM_OP_STRING_TO_BOOLS in nl_classifier_test_utils for details.
constexpr char kTestModelBoolOutputPath[] =
    "test_model_nl_classifier_bool_output.tflite";

// The model has same input/output tensors with the above model, except its
// first output tensor is associated with metadata with name
// kMetadataOutputScoreTensorName and an associated label file.
constexpr char kTestModelWithLabelCustomOpsPath[] =
    "test_model_nl_classifier_with_associated_label.tflite";

constexpr char kTestModelWithLabelBuiltInOpsPath[] =
    "test_model_nl_classifier_with_associated_label_builtin_ops.tflite";

// This model expects input to be tokenized by a regex tokenizer.
constexpr char kTestModelWithRegexTokenizer[] =
    "test_model_nl_classifier_with_regex_tokenizer.tflite";

constexpr char kPositiveInput[] =
    "This is the best movie I’ve seen in recent years. Strongly recommend "
    "it!";
std::vector<core::Category> GetExpectedResultsOfPositiveInput() {
  return {
      {"Positive", 0.51342660188674927},
      {"Negative", 0.48657345771789551},
  };
}

constexpr char kNegativeInput[] = "What a waste of my time.";
std::vector<core::Category> GetExpectedResultsOfNegativeInput() {
  return {
      {"Positive", 0.18687039613723755},
      {"Negative", 0.81312954425811768},
  };
}

const uint8_t kOutputDequantizedTensorIndex = 0;
const uint8_t kOutputQuantizedTensorIndex = 1;
const uint8_t kOutputLabelTensorIndex = 2;
const uint8_t kOutputDequantizedTensorFloat64Index = 3;
constexpr char kInputTensorName[] = "INPUT";
constexpr char kOutputDequantizedTensorName[] = "OUTPUT_SCORE_DEQUANTIZED";
constexpr char kOutputDequantizedTensorFloat64Name[] =
    "OUTPUT_SCORE_DEQUANTIZED_FLOAT64";
constexpr char kOutputQuantizedTensorName[] = "OUTPUT_SCORE_QUANTIZED";
constexpr char kOutputLabelTensorName[] = "LABELS";
constexpr char kMetadataOutputScoreTensorName[] = "scores_dequantized";
constexpr char kDefaultInputTensorName[] = "INPUT";
constexpr char kDefaultOutputLabelTensorName[] = "OUTPUT_LABEL";
constexpr int kDefaultInputTensorIndex = 0;
constexpr int kDefaultOutputLabelTensorIndex = -1;

// Test the API with different combinations in creating proto
// NLClassifierOptions
struct ProtoOptionsTestParam {
  // description of current test
  std::string description;
  NLClassifierProtoOptions options;
};

std::string GetFullPath(absl::string_view file_name) {
  return JoinPath("./" /*test src dir*/, kTestDataDirectory,
                  file_name);
}

class ProtoOptionsTest : public TestWithParam<ProtoOptionsTestParam> {
 protected:
  void SetUp() override { ASSERT_EQ(TfLiteInitializeShimsForTest(), 0); }
};

TEST_F(ProtoOptionsTest, CreateFromOptionsSucceedsWithModelWithMetadata) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelWithRegexTokenizer));

  SUPPORT_ASSERT_OK(NLClassifier::CreateFromOptions(options));
}

TEST_F(ProtoOptionsTest, CreateFromOptionsFailsWithMissingBaseOptions) {
  NLClassifierProtoOptions options;
  StatusOr<std::unique_ptr<NLClassifier>> nl_classifier_or =
      NLClassifier::CreateFromOptions(options);

  EXPECT_EQ(nl_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(nl_classifier_or.status().message(),
              HasSubstr("Missing mandatory `base_options`"));
  EXPECT_THAT(nl_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord(
                  absl::StrCat(TfLiteSupportStatus::kInvalidArgumentError))));
}

TEST_F(ProtoOptionsTest, ClassifySucceedsWithBaseOptions) {
  std::unique_ptr<NLClassifier> classifier;

  // Test creating NLClassifier when classifier outlives options.
  {
    std::string contents =
        LoadBinaryContent(GetFullPath(kTestModelWithRegexTokenizer).c_str());
    NLClassifierProtoOptions options;
    options.mutable_base_options()->mutable_model_file()->set_file_content(
        contents);

    SUPPORT_ASSERT_OK_AND_ASSIGN(classifier, NLClassifier::CreateFromOptions(options));
  }

  std::vector<core::Category> positive_results =
      classifier->Classify(kPositiveInput);

  EXPECT_THAT(positive_results,
              UnorderedElementsAreArray(GetExpectedResultsOfPositiveInput()));

  std::vector<core::Category> negative_results =
      classifier->Classify(kNegativeInput);
  EXPECT_THAT(negative_results,
              UnorderedElementsAreArray(GetExpectedResultsOfNegativeInput()));
}

TEST_F(ProtoOptionsTest, CreationFromIncorrectInputTensor) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kTestModelPath));
  options.set_input_tensor_name("invalid_tensor_name");
  options.set_input_tensor_index(-1);

  StatusOr<std::unique_ptr<NLClassifier>> nl_classifier_or =
      NLClassifier::CreateFromOptions(options, CreateCustomResolver());

  EXPECT_EQ(nl_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(nl_classifier_or.status().message(),
              HasSubstr("No input tensor found with name "
                        "invalid_tensor_name or at index -1"));
  EXPECT_THAT(
      nl_classifier_or.status().GetPayload(kTfLiteSupportPayload),
      absl::Cord(absl::StrCat(TfLiteSupportStatus::kInputTensorNotFoundError)));
}

TEST_F(ProtoOptionsTest, CreationFromIncorrectOutputScoreTensor) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(JoinPath(
      "./" /*test src dir*/, kTestDataDirectory, kTestModelPath));
  options.set_output_score_tensor_name("invalid_tensor_name");
  options.set_output_score_tensor_index(-1);

  StatusOr<std::unique_ptr<NLClassifier>> nl_classifier_or =
      NLClassifier::CreateFromOptions(options, CreateCustomResolver());

  EXPECT_EQ(nl_classifier_or.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(nl_classifier_or.status().message(),
              HasSubstr("No output score tensor found with name "
                        "invalid_tensor_name or at index -1"));
  EXPECT_THAT(nl_classifier_or.status().GetPayload(kTfLiteSupportPayload),
              absl::Cord(absl::StrCat(
                  TfLiteSupportStatus::kOutputTensorNotFoundError)));
}

TEST_F(ProtoOptionsTest, TestInferenceWithRegexTokenizer) {
  // The model with regex tokenizer doesn't need any custom ops.
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelWithRegexTokenizer));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<NLClassifier> classifier,
                       NLClassifier::CreateFromOptions(options));

  std::vector<core::Category> positive_results =
      classifier->Classify(kPositiveInput);

  EXPECT_THAT(positive_results,
              UnorderedElementsAreArray(GetExpectedResultsOfPositiveInput()));

  std::vector<core::Category> negative_results =
      classifier->Classify(kNegativeInput);
  EXPECT_THAT(negative_results,
              UnorderedElementsAreArray(GetExpectedResultsOfNegativeInput()));
}

TEST_F(ProtoOptionsTest, TestInferenceWithBoolOutput) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelBoolOutputPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<NLClassifier> classifier,
      NLClassifier::CreateFromOptions(options, CreateCustomResolver()));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"0", 1},
      {"1", 1},
      {"2", 0},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

TEST_F(ProtoOptionsTest, TestInferenceWithAssociatedLabelCustomOps) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelWithLabelCustomOpsPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<NLClassifier> classifier,
      NLClassifier::CreateFromOptions(options, CreateCustomResolver()));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"label0", 255},
      {"label1", 510},
      {"label2", 765},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

TEST_F(ProtoOptionsTest, TestInferenceWithAssociatedLabelBuiltinOps) {
  NLClassifierProtoOptions options;
  options.mutable_base_options()->mutable_model_file()->set_file_name(
      GetFullPath(kTestModelWithLabelBuiltInOpsPath));
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<NLClassifier> classifier,
                       NLClassifier::CreateFromOptions(options));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"Negative", 0.49332118034362793},
      {"Positive", 0.50667881965637207},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

// Parameterized test.
struct ProtoOptionsTestParamToString {
  std::string operator()(
      const ::testing::TestParamInfo<ProtoOptionsTestParam>& info) const {
    return info.param.description;
  }
};

NLClassifierProtoOptions CreateProtoOptionsFromTensorName(
    const char* input_tensor_name, const char* output_score_tensor_name,
    const char* output_label_tensor_name, const char* model_path) {
  NLClassifierProtoOptions options;
  options.set_input_tensor_name(input_tensor_name);
  options.set_output_score_tensor_name(output_score_tensor_name);
  options.set_output_label_tensor_name(output_label_tensor_name);

  options.mutable_base_options()->mutable_model_file()->set_file_name(
      model_path);

  return options;
}

NLClassifierProtoOptions CreateProtoOptionsFromTensorIndex(
    const int input_tensor_index, const int output_score_tensor_index,
    const int output_label_tensor_index, const char* model_path) {
  NLClassifierProtoOptions options;
  options.set_input_tensor_index(input_tensor_index);
  options.set_output_score_tensor_index(output_score_tensor_index);
  options.set_output_label_tensor_index(output_label_tensor_index);

  options.mutable_base_options()->mutable_model_file()->set_file_name(
      model_path);

  return options;
}

std::vector<ProtoOptionsTestParam> ClassifyParams() {
  return {
      {
          /* description= */ "FindTensorByNameQuantizeOutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(
              kDefaultInputTensorName, kOutputQuantizedTensorName,
              kOutputLabelTensorName, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByNameQuantizeOutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(kDefaultInputTensorName,
                                           kOutputQuantizedTensorName,
                                           kDefaultOutputLabelTensorName,
                                           GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByNameDequantizeOutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(
              kDefaultInputTensorName, kOutputDequantizedTensorName,
              kOutputLabelTensorName, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByNameDequantizeOutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(kDefaultInputTensorName,
                                           kOutputDequantizedTensorName,
                                           kDefaultOutputLabelTensorName,
                                           GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */
          "FindTensorByNameDequantizeFloat64OutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(
              kDefaultInputTensorName, kOutputDequantizedTensorFloat64Name,
              kOutputLabelTensorName, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */
          "FindTensorByNameDequantizeFloat64OutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorName(kDefaultInputTensorName,
                                           kOutputDequantizedTensorFloat64Name,
                                           kDefaultOutputLabelTensorName,
                                           GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByIndexQuantizeOutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputQuantizedTensorIndex,
              kOutputLabelTensorIndex, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByIndexQuantizeOutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputQuantizedTensorIndex,
              kDefaultOutputLabelTensorIndex,
              GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByIndexDequantizeOutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputDequantizedTensorIndex,
              kOutputLabelTensorIndex, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */ "FindTensorByIndexDequantizeOutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputDequantizedTensorIndex,
              kDefaultOutputLabelTensorIndex,
              GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */
          "FindTensorByIndexDequantizeFloat64OutputUseTensorLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputDequantizedTensorFloat64Index,
              kOutputLabelTensorIndex, GetFullPath(kTestModelPath).c_str()),
      },
      {
          /* description= */
          "FindTensorByIndexDequantizeFloat64OutputUseIndexLabel",
          /* options= */
          CreateProtoOptionsFromTensorIndex(
              kDefaultInputTensorIndex, kOutputDequantizedTensorFloat64Index,
              kDefaultOutputLabelTensorIndex,
              GetFullPath(kTestModelPath).c_str()),
      },
  };
}

TEST_P(ProtoOptionsTest, TestClassify) {
  NLClassifierProtoOptions options = GetParam().options;

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<NLClassifier> classifier,
      NLClassifier::CreateFromOptions(options, CreateCustomResolver()));
  std::vector<core::Category> results = classifier->Classify(kInputStr);

  bool assert_label_name =
      options.output_label_tensor_index() == kOutputLabelTensorIndex ||
      options.output_label_tensor_name() == kOutputLabelTensorName;

  std::vector<core::Category> expected_class;
  if (assert_label_name) {
    expected_class = {
        {"label0", 255},
        {"label1", 510},
        {"label2", 765},
    };
  } else {
    expected_class = {
        {"0", 255},
        {"1", 510},
        {"2", 765},
    };
  }

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

INSTANTIATE_TEST_SUITE_P(TestClassify, ProtoOptionsTest,
                         ValuesIn(ClassifyParams()),
                         ProtoOptionsTestParamToString());

// Tests for struct sNLClassifierOptions.
class StructOptionsTest : public tflite::testing::Test {};

void AssertStatus(absl::Status status, absl::StatusCode status_code,
                  TfLiteSupportStatus tfls_code) {
  ASSERT_EQ(status.code(), status_code);
  EXPECT_THAT(status.GetPayload(kTfLiteSupportPayload),
              ::testing::Optional(absl::Cord(absl::StrCat(tfls_code))));
}

TEST_F(StructOptionsTest, TestApiCreationFromBuffer) {
  std::string model_buffer =
      LoadBinaryContent(JoinPath("./" /*test src dir*/,
                                 kTestDataDirectory, kTestModelPath)
                            .c_str());
  SUPPORT_ASSERT_OK(NLClassifier::CreateFromBufferAndOptions(
      model_buffer.data(), model_buffer.size(), {}, CreateCustomResolver()));
}

TEST_F(StructOptionsTest, TestApiCreationFromFile) {
  SUPPORT_ASSERT_OK(NLClassifier::CreateFromFileAndOptions(GetFullPath(kTestModelPath),
                                                   {}, CreateCustomResolver()));
}

TEST_F(StructOptionsTest, TestApiCreationFromIncorrectInputTensor) {
  NLClassifierOptions options;
  options.input_tensor_index = -1;
  options.input_tensor_name = "I do not exist";
  AssertStatus(NLClassifier::CreateFromFileAndOptions(
                   JoinPath("./" /*test src dir*/,
                            kTestDataDirectory, kTestModelPath),
                   options, CreateCustomResolver())
                   .status(),
               absl::StatusCode::kInvalidArgument,
               TfLiteSupportStatus::kInputTensorNotFoundError);
}

TEST_F(StructOptionsTest, TestApiCreationFromIncorrectOutputScoreTensor) {
  NLClassifierOptions options;
  options.output_score_tensor_index = 123;

  AssertStatus(NLClassifier::CreateFromFileAndOptions(
                   GetFullPath(kTestModelPath), options, CreateCustomResolver())
                   .status(),
               absl::StatusCode::kInvalidArgument,
               TfLiteSupportStatus::kOutputTensorNotFoundError);
}

TEST_F(StructOptionsTest, TestInferenceWithRegexTokenizer) {
  NLClassifierOptions options;
  options.input_tensor_name = "input_text";
  options.output_score_tensor_name = "probability";

  // The model with regex tokenizer doesn't need any custom ops.
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<NLClassifier> classifier,
                       NLClassifier::CreateFromFileAndOptions(
                           GetFullPath(kTestModelWithRegexTokenizer), options));

  std::vector<core::Category> positive_results =
      classifier->Classify(kPositiveInput);

  EXPECT_THAT(positive_results,
              UnorderedElementsAreArray(GetExpectedResultsOfPositiveInput()));

  std::vector<core::Category> negative_results =
      classifier->Classify(kNegativeInput);
  EXPECT_THAT(negative_results,
              UnorderedElementsAreArray(GetExpectedResultsOfNegativeInput()));
}

TEST_F(StructOptionsTest, TestInferenceWithBoolOutput) {
  NLClassifierOptions options;
  options.input_tensor_index = 0;
  options.output_score_tensor_index = 0;

  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<NLClassifier> classifier,
                       NLClassifier::CreateFromFileAndOptions(
                           GetFullPath(kTestModelBoolOutputPath), options,
                           CreateCustomResolver()));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"0", 1},
      {"1", 1},
      {"2", 0},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

TEST_F(StructOptionsTest, TestInferenceWithAssociatedLabelCustomOps) {
  NLClassifierOptions options;
  options.output_score_tensor_name = kMetadataOutputScoreTensorName;
  SUPPORT_ASSERT_OK_AND_ASSIGN(std::unique_ptr<NLClassifier> classifier,
                       NLClassifier::CreateFromFileAndOptions(
                           GetFullPath(kTestModelWithLabelCustomOpsPath),
                           options, CreateCustomResolver()));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"label0", 255},
      {"label1", 510},
      {"label2", 765},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

TEST_F(StructOptionsTest, TestInferenceWithAssociatedLabelBuiltinOps) {
  NLClassifierOptions options;
  options.input_tensor_index = 0;
  options.output_score_tensor_index = 0;
  SUPPORT_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<NLClassifier> classifier,
      NLClassifier::CreateFromFileAndOptions(
          GetFullPath(kTestModelWithLabelBuiltInOpsPath), options));
  std::vector<core::Category> results = classifier->Classify(kInputStr);
  std::vector<core::Category> expected_class = {
      {"Negative", 0.49332118034362793},
      {"Positive", 0.50667881965637207},
  };

  EXPECT_THAT(results, UnorderedElementsAreArray(expected_class));
}

}  // namespace
}  // namespace nlclassifier
}  // namespace text
}  // namespace task
}  // namespace tflite
