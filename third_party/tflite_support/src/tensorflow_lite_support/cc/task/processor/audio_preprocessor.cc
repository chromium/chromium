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
#include "tensorflow_lite_support/cc/task/processor/audio_preprocessor.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/audio/core/audio_buffer.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"

namespace tflite {
namespace task {
namespace processor {

namespace {
// Looks up AudioProperty from metadata. If no error occurs, the returned value
// is guaranteed to be valid (not null).
tflite::support::StatusOr<const AudioProperties*> GetAudioPropertiesSafe(
    const TensorMetadata* tensor_metadata, int input_index) {
  if (tensor_metadata->content() == nullptr ||
      tensor_metadata->content()->content_properties() == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        "Missing audio format metadata in the model metadata.",
        tflite::support::TfLiteSupportStatus::kMetadataNotFoundError);
  }

  ContentProperties type =
      tensor_metadata->content()->content_properties_type();

  if (type != ContentProperties_AudioProperties) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrCat("Expected AudioProperties for tensor ",
                     tensor_metadata->name()
                         ? tensor_metadata->name()->str()
                         : absl::StrFormat("#%d", input_index),
                     ", got ", EnumNameContentProperties(type), "."),
        tflite::support::TfLiteSupportStatus::
            kMetadataInvalidContentPropertiesError);
  }

  auto props =
      tensor_metadata->content()->content_properties_as_AudioProperties();
  if (props == nullptr) {
    return support::CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrCat("Expected AudioProperties for tensor",
                     tensor_metadata->name()
                         ? tensor_metadata->name()->str()
                         : absl::StrFormat("#%d", input_index),
                     ", got nullptr"),
        tflite::support::TfLiteSupportStatus::
            kMetadataInvalidContentPropertiesError);
  }
  return props;
}
}  // namespace

/* static */
tflite::support::StatusOr<std::unique_ptr<AudioPreprocessor>>
AudioPreprocessor::Create(tflite::task::core::TfLiteEngine* engine,
                          const std::initializer_list<int> input_indices) {
  TFLITE_ASSIGN_OR_RETURN(auto processor,
                   Processor::Create<AudioPreprocessor>(
                       /* num_expected_tensors = */ 1, engine, input_indices));

  TFLITE_RETURN_IF_ERROR(processor->Init());
  return processor;
}

absl::Status AudioPreprocessor::Init() {
  TFLITE_RETURN_IF_ERROR(SetAudioFormatFromMetadata());
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());
  return absl::OkStatus();
}

absl::Status AudioPreprocessor::SetAudioFormatFromMetadata() {
  TFLITE_ASSIGN_OR_RETURN(
      const AudioProperties* props,
      GetAudioPropertiesSafe(GetTensorMetadata(), tensor_indices_.at(0)));
  audio_format_.channels = props->channels();
  audio_format_.sample_rate = props->sample_rate();
  if (audio_format_.channels <= 0 || audio_format_.sample_rate <= 0) {
    return tflite::support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Missing audio format metadata in the model.",
        tflite::support::TfLiteSupportStatus::kMetadataNotFoundError);
  }

  return absl::OkStatus();
}

absl::Status AudioPreprocessor::CheckAndSetInputs() {
  input_buffer_size_ = 1;
  for (int i = 0; i < GetTensor()->dims->size; i++) {
    if (GetTensor()->dims->data[i] < 1) {
      return CreateStatusWithPayload(
          absl::StatusCode::kInvalidArgument,
          absl::StrFormat("Invalid size: %d for input tensor dimension: %d.",
                          GetTensor()->dims->data[i], i),
          tflite::support::TfLiteSupportStatus::
              kInvalidInputTensorDimensionsError);
    }
    input_buffer_size_ *= GetTensor()->dims->data[i];
  }
  // Check if the input buffer size is divisible by the required audio channels.
  // This needs to be done after loading metadata and input.
  if (input_buffer_size_ % audio_format_.channels != 0) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInternal,
        absl::StrFormat("Model input tensor size (%d) should be a "
                        "multiplier of the number of channels (%d).",
                        input_buffer_size_, audio_format_.channels),
        tflite::support::TfLiteSupportStatus::kMetadataInconsistencyError);
  }
  return absl::OkStatus();
}

absl::Status AudioPreprocessor::Preprocess(
    const ::tflite::task::audio::AudioBuffer& audio_buffer) {
  if (audio_buffer.GetAudioFormat().channels != audio_format_.channels) {
    return tflite::support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Input audio buffer channel number %d does not match "
                        "the model required audio channel number %d.",
                        audio_buffer.GetAudioFormat().channels,
                        audio_format_.channels));
  }
  if (audio_buffer.GetAudioFormat().sample_rate != audio_format_.sample_rate) {
    return tflite::support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Input audio sample rate %d does not match "
                        "the model required audio sample rate %d.",
                        audio_buffer.GetAudioFormat().sample_rate,
                        audio_format_.sample_rate));
  }
  if (audio_buffer.GetBufferSize() != input_buffer_size_) {
    return tflite::support::CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Input audio buffer size %d does not match the model required "
            "input size %d.",
            audio_buffer.GetBufferSize(), input_buffer_size_),
        tflite::support::TfLiteSupportStatus::kInvalidArgumentError);
  }
  return tflite::task::core::PopulateTensor(audio_buffer.GetFloatBuffer(),
                                            input_buffer_size_, GetTensor());
}

}  // namespace processor
}  // namespace task
}  // namespace tflite
