/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/c/task/audio/audio_classifier.h"

#include <string.h>

#include "tensorflow/lite/core/shims/cc/shims_test_util.h"
#include "tensorflow_lite_support/c/common.h"
#include "tensorflow_lite_support/c/task/audio/core/audio_buffer.h"
#include "tensorflow_lite_support/c/task/processor/classification_result.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/audio/utils/wav_io.h"
#include "tensorflow_lite_support/cc/test/test_utils.h"

namespace tflite {
namespace task {
namespace audio {
namespace {

using ::testing::HasSubstr;
using ::tflite::support::StatusOr;
using ::tflite::task::JoinPath;

constexpr char kTestDataDirectory[] =
    "/tensorflow_lite_support/cc/test/testdata/task/"
    "audio/";
// Quantized model.
constexpr char kYamNetAudioClassifierWithMetadata[] =
    "yamnet_audio_classifier_with_metadata.tflite";

StatusOr<TfLiteAudioBuffer> LoadAudioBufferFromFileNamed(
    const std::string wav_file,
    int buffer_size) {
  std::string contents =
      ReadFile(JoinPath("./" /*test src dir*/, kTestDataDirectory, wav_file));

  uint32_t decoded_sample_count;
  uint16_t decoded_channel_count;
  uint32_t decoded_sample_rate;
  std::vector<float> wav_data;

  absl::Status read_audio_file_status = DecodeLin16WaveAsFloatVector(
      contents, &wav_data, &decoded_sample_count, &decoded_channel_count,
      &decoded_sample_rate);

  if (decoded_sample_count > buffer_size) {
    decoded_sample_count = buffer_size;
  }

  if (!read_audio_file_status.ok()) {
    return read_audio_file_status;
  }

  float* c_wav_data = (float*)malloc(sizeof(float) * wav_data.size());
  if (!c_wav_data) {
    exit(-1);
  }

  memcpy(c_wav_data, wav_data.data(), sizeof(float) * wav_data.size());

  TfLiteAudioBuffer audio_buffer = {
      .format = {.channels = decoded_channel_count,
                 .sample_rate = static_cast<int>(decoded_sample_rate)},
      .data = c_wav_data,
      .size = static_cast<int>(decoded_sample_count)};

  return audio_buffer;
}

void Verify(TfLiteClassificationResult* classification_result,
            int expected_classifications_size) {
  EXPECT_NE(classification_result, nullptr);
  EXPECT_EQ(classification_result->size, expected_classifications_size);
  EXPECT_NE(classification_result->classifications, nullptr);
}

void Verify(TfLiteClassifications& classifications,
            int expected_categories_size,
            int expected_head_index,
            char const* expected_head_name) {
  EXPECT_EQ(classifications.size, expected_categories_size);
  EXPECT_EQ(classifications.head_index, expected_head_index);
  ASSERT_NE(classifications.head_name, nullptr);
  if (expected_head_name) {
    EXPECT_EQ(strcmp(classifications.head_name, expected_head_name), 0);
  }
  EXPECT_NE(classifications.categories, nullptr);
}

void Verify(TfLiteCategory& category,
            int expected_index,
            char const* expected_label,
            float expected_score) {
  const float kPrecision = 1e-6;
  EXPECT_EQ(category.index, expected_index);
  EXPECT_NE(category.label, nullptr);

  if (category.label && expected_label) {
    EXPECT_EQ(strcmp(category.label, expected_label), 0);
  }

  EXPECT_EQ(category.display_name, nullptr);
  EXPECT_NEAR(category.score, expected_score, kPrecision);
}

void Verify(TfLiteSupportError* error,
            TfLiteSupportErrorCode error_code,
            char const* message) {
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->code, kInvalidArgumentError);
  EXPECT_NE(error->message, nullptr);
  EXPECT_THAT(error->message, HasSubstr(message));
}

class AudioClassifierFromOptionsTest : public tflite_shims::testing::Test {};

TEST_F(AudioClassifierFromOptionsTest, FailsWithMissingModelPathAndError) {
  TfLiteAudioClassifierOptions options = TfLiteAudioClassifierOptionsCreate();

  TfLiteSupportError* error = nullptr;
  TfLiteAudioClassifier* audio_classifier =
      TfLiteAudioClassifierFromOptions(&options, &error);

  EXPECT_EQ(audio_classifier, nullptr);
  if (audio_classifier)
    TfLiteAudioClassifierDelete(audio_classifier);

  Verify(error, kInvalidArgumentError,
         "INVALID_ARGUMENT: Missing mandatory `model_file` field in "
         "`base_options`");

  TfLiteSupportErrorDelete(error);
}

TEST_F(AudioClassifierFromOptionsTest, SucceedsWithModelPath) {
  std::string model_path = JoinPath("./" /*test src dir*/, kTestDataDirectory,
                                    kYamNetAudioClassifierWithMetadata);
  TfLiteAudioClassifierOptions options = TfLiteAudioClassifierOptionsCreate();
  options.base_options.model_file.file_path = model_path.data();
  TfLiteAudioClassifier* audio_classifier =
      TfLiteAudioClassifierFromOptions(&options, nullptr);

  EXPECT_NE(audio_classifier, nullptr);
  TfLiteAudioClassifierDelete(audio_classifier);
}

class AudioClassifierClassifyTest : public tflite_shims::testing::Test {
 protected:
  void SetUp() override {
    std::string model_path = JoinPath("./" /*test src dir*/, kTestDataDirectory,
                                      kYamNetAudioClassifierWithMetadata);

    TfLiteAudioClassifierOptions options = TfLiteAudioClassifierOptionsCreate();
    options.base_options.model_file.file_path = model_path.data();
    audio_classifier = TfLiteAudioClassifierFromOptions(&options, nullptr);
    ASSERT_NE(audio_classifier, nullptr);
  }

  void TearDown() override { TfLiteAudioClassifierDelete(audio_classifier); }
  TfLiteAudioClassifier* audio_classifier;
};

TEST_F(AudioClassifierClassifyTest, SucceedsWithAudioFile) {
  int input_buffer_size = TfLiteAudioClassifierGetRequiredInputBufferSize(
      audio_classifier, nullptr);
  ASSERT_NE(input_buffer_size, -1);

  SUPPORT_ASSERT_OK_AND_ASSIGN(
      TfLiteAudioBuffer audio_buffer,
      LoadAudioBufferFromFileNamed("speech.wav", input_buffer_size));

  TfLiteSupportError* classifyError = NULL;
  TfLiteClassificationResult* classification_result =
      TfLiteAudioClassifierClassify(audio_classifier, &audio_buffer,
                                    &classifyError);

  TfLiteAudioBufferDeleteData(audio_buffer);

  Verify(classification_result, 1);
  Verify(classification_result->classifications[0], 521, 0, "scores");
  Verify(classification_result->classifications[0].categories[0], 0, "Speech",
         0.917969);
  Verify(classification_result->classifications[0].categories[1], 500,
         "Inside, small room", 0.058594);
  Verify(classification_result->classifications[0].categories[2], 494,
         "Silence", 0.011719);

  TfLiteClassificationResultDelete(classification_result);
}

}  // namespace
}  // namespace audio
}  // namespace task
}  // namespace tflite
