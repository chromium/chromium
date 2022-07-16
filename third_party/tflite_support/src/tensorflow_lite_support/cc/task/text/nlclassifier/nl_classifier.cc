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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/core/api/op_resolver.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/category.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/text/tokenizers/regex_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer.h"
#include "tensorflow_lite_support/cc/utils/common_utils.h"

namespace tflite {
namespace task {
namespace text {
namespace nlclassifier {

using ::absl::StatusCode;
using ::flatbuffers::Offset;
using ::flatbuffers::Vector;
using ::tflite::TensorMetadata;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::support::text::tokenizer::RegexTokenizer;
using ::tflite::support::text::tokenizer::Tokenizer;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::support::utils::LoadVocabFromBuffer;
using ::tflite::task::core::Category;
using ::tflite::task::core::Dequantize;
using ::tflite::task::core::GetStringAtIndex;
using ::tflite::task::core::PopulateTensor;

namespace {
constexpr int kRegexTokenizerInputTensorIndex = 0;
constexpr int kRegexTokenizerProcessUnitIndex = 0;

StatusOr<absl::string_view> CheckAndLoadFirstAssociatedFile(
    const flatbuffers::Vector<flatbuffers::Offset<tflite::AssociatedFile>>*
        associated_files,
    const tflite::metadata::ModelMetadataExtractor* metadata_extractor) {
  if (associated_files == nullptr || associated_files->size() < 1 ||
      associated_files->Get(0)->name() == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Invalid vocab_file from input process unit.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                   metadata_extractor->GetAssociatedFile(
                       associated_files->Get(0)->name()->str()));
  return vocab_buffer;
}

StatusOr<std::unique_ptr<Tokenizer>> CreateRegexTokenizerFromProcessUnit(
    const tflite::ProcessUnit* tokenizer_process_unit,
    const tflite::metadata::ModelMetadataExtractor* metadata_extractor) {
  if (metadata_extractor == nullptr || tokenizer_process_unit == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No metadata or input process unit found.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }

  if (tokenizer_process_unit->options_type() !=
      ProcessUnitOptions_RegexTokenizerOptions) {
    return CreateStatusWithPayload(
        absl::StatusCode::kNotFound,
        absl::StrCat(
            "Incorrect options_type:", tokenizer_process_unit->options_type(),
            " need RegexTokenizerOptions."),
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }

  const tflite::RegexTokenizerOptions* options =
      tokenizer_process_unit->options_as<RegexTokenizerOptions>();
  ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                   CheckAndLoadFirstAssociatedFile(options->vocab_file(),
                                                   metadata_extractor));
  if (options->delim_regex_pattern() == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Invalid delim_regex_pattern from input process unit.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }

  std::unique_ptr<RegexTokenizer> regex_tokenizer =
      absl::make_unique<RegexTokenizer>(options->delim_regex_pattern()->str(),
                                        vocab_buffer.data(),
                                        vocab_buffer.size());

  int unknown_token_id = 0;
  if (!regex_tokenizer->GetUnknownToken(&unknown_token_id)) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "RegexTokenizer doesn't have <UNKNOWN> token.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }

  int pad_token_id = 0;
  if (!regex_tokenizer->GetPadToken(&pad_token_id)) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "RegexTokenizer doesn't have <PAD> token.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  return regex_tokenizer;
}

}  // namespace

const NLClassifierOptions& NLClassifier::GetOptions() const {
  return options_;
}

absl::Status NLClassifier::TrySetLabelFromMetadata(
    const TensorMetadata* metadata) {
  if (metadata == nullptr) {
    return CreateStatusWithPayload(absl::StatusCode::kInvalidArgument,
                                   "Metadata not found for output tensor",
                                   TfLiteSupportStatus::kMetadataNotFoundError);
  }
  const auto* associated_files = metadata->associated_files();
  if (associated_files == nullptr || associated_files->size() == 0) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No label file found for tensor metadata.",
        TfLiteSupportStatus::kMetadataMissingLabelsError);
  }
  const tflite::AssociatedFile* associated_file =
      associated_files->Get(kOutputTensorLabelFileIndex);
  if (associated_file->type() != AssociatedFileType_TENSOR_AXIS_LABELS) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Incorrect label type found for tensor metadata.",
        TfLiteSupportStatus::kMetadataMissingLabelsError);
  }
  tflite::support::StatusOr<absl::string_view> label_buffer =
      GetMetadataExtractor()->GetAssociatedFile(
          associated_files->Get(kOutputTensorIndex)->name()->str());
  if (label_buffer.ok()) {
    labels_vector_ =
        absl::make_unique<std::vector<std::string>>(LoadVocabFromBuffer(
            label_buffer.value().data(), label_buffer.value().size()));
    return absl::OkStatus();
  } else {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Failed to extract label file from metadata.",
        TfLiteSupportStatus::kMetadataMissingLabelsError);
  }
}

std::vector<Category> NLClassifier::Classify(const std::string& text) {
  // The NLClassifier implementation for Preprocess() and Postprocess() never
  // returns errors: just call value().
  return Infer(text).value();
}

absl::Status NLClassifier::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::string& input) {
  TfLiteTensor* input_tensor = FindTensorWithNameOrIndex(
      input_tensors, GetMetadataExtractor()->GetInputTensorMetadata(),
      options_.input_tensor_name, options_.input_tensor_index);
  if (input_tensor == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No input tensor found from NLClassifierOptions.",
        TfLiteSupportStatus::kInputTensorNotFoundError);
  }

  if (HasRegexTokenizerMetadata()) {
    //                              |<-------sentence_length-------->|
    // input_tensor                 <START>, t1, t2... <PAD>, <PAD>...
    // <START> is optional, t1, t2... will be replaced by <UNKNOWN> if it's not
    // found in tokenizer vocab.
    TokenizerResult result = tokenizer_->Tokenize(input);

    size_t max_sentence_length = input_tensor->dims->size == 2
                                     ? input_tensor->dims->data[1]
                                     : input_tensor->dims->data[0];

    int unknown_token_id = 0;
    tokenizer_->GetUnknownToken(&unknown_token_id);

    int pad_token_id = 0;
    tokenizer_->GetPadToken(&pad_token_id);

    std::vector<int> input_tokens(max_sentence_length, pad_token_id);
    int start_token_id = 0;
    size_t input_token_index = 0;
    if (tokenizer_->GetStartToken(&start_token_id)) {
      input_tokens[0] = start_token_id;
      input_token_index = 1;
    }

    for (size_t i = 0; (i < result.subwords.size()) &&
                       (input_token_index < max_sentence_length);
         ++i, ++input_token_index) {
      const std::string& token = result.subwords[i];
      int token_id = 0;
      if (tokenizer_->LookupId(token, &token_id)) {
        input_tokens[input_token_index] = token_id;
      } else {
        input_tokens[input_token_index] = unknown_token_id;
      }
    }

    PopulateTensor(input_tokens, input_tensor);
  } else {
    PopulateTensor(input, input_tensor);
  }
  return absl::OkStatus();
}

StatusOr<std::vector<Category>> NLClassifier::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const std::string& /*input*/) {
  return BuildResults(
      FindTensorWithNameOrIndex(
          output_tensors, GetMetadataExtractor()->GetOutputTensorMetadata(),
          options_.output_score_tensor_name,
          options_.output_score_tensor_index),
      FindTensorWithNameOrIndex(
          output_tensors, GetMetadataExtractor()->GetInputTensorMetadata(),
          options_.output_label_tensor_name,
          options_.output_label_tensor_index));
}

std::vector<Category> NLClassifier::BuildResults(const TfLiteTensor* scores,
                                                 const TfLiteTensor* labels) {
  bool use_index_as_labels = (labels_vector_ == nullptr) && (labels == nullptr);
  // Some models output scores with transposed shape [1, categories]
  int categories =
      scores->dims->size == 2 ? scores->dims->data[1] : scores->dims->data[0];

  std::vector<Category> predictions;
  predictions.reserve(categories);

  bool should_dequantize = scores->type == kTfLiteUInt8 ||
                           scores->type == kTfLiteInt8 ||
                           scores->type == kTfLiteInt16;
  for (int index = 0; index < categories; index++) {
    std::string label;
    if (use_index_as_labels) {
      label = std::to_string(index);
    } else if (labels_vector_ == nullptr) {
      if (labels->type == kTfLiteString) {
        label = GetStringAtIndex(labels, index);
      } else if (labels->type == kTfLiteInt32) {
        label = std::to_string(GetTensorData<int>(labels)[index]);
      }
    } else {
      label = (*labels_vector_)[index];
    }
    if (should_dequantize) {
      predictions.push_back(Category(label, Dequantize(*scores, index)));
    } else if (scores->type == kTfLiteBool) {
      predictions.push_back(
          Category(label, GetTensorData<bool>(scores)[index] ? 1.0 : 0.0));
    } else {
      predictions.push_back(
          Category(label, scores->type == kTfLiteFloat32
                              ? GetTensorData<float>(scores)[index]
                              : GetTensorData<double>(scores)[index]));
    }
  }

  return predictions;
}
absl::Status NLClassifier::Initialize(const NLClassifierOptions& options) {
  options_ = options;
  // input tensor should be type STRING
  auto input_tensor = FindTensorWithNameOrIndex(
      GetInputTensors(), GetMetadataExtractor()->GetInputTensorMetadata(),
      options.input_tensor_name, options.input_tensor_index);
  if (input_tensor == nullptr) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("No input tensor found with name ",
                     options.input_tensor_name, " or at index ",
                     options.input_tensor_index),
        TfLiteSupportStatus::kInputTensorNotFoundError);
  }
  if (HasRegexTokenizerMetadata()) {
    if (input_tensor->type != kTfLiteInt32) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrCat("Type mismatch for input tensor ", input_tensor->name,
                       ". Requested INT32, got ",
                       TfLiteTypeGetName(input_tensor->type), "."),
          TfLiteSupportStatus::kInvalidInputTensorTypeError);
    }
    RETURN_IF_ERROR(SetupRegexTokenizer());
  } else {
    if (input_tensor->type != kTfLiteString) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrCat("Type mismatch for input tensor ", input_tensor->name,
                       ". Requested STRING, got ",
                       TfLiteTypeGetName(input_tensor->type), "."),
          TfLiteSupportStatus::kInvalidInputTensorTypeError);
    }
  }

  // output score tensor should be type
  // UINT8/INT8/INT16(quantized) or FLOAT32/FLOAT64(dequantized) or BOOL
  std::vector<const TfLiteTensor*> output_tensors = GetOutputTensors();
  const Vector<Offset<TensorMetadata>>* output_tensor_metadatas =
      GetMetadataExtractor()->GetOutputTensorMetadata();

  const auto scores = FindTensorWithNameOrIndex(
      output_tensors, output_tensor_metadatas, options.output_score_tensor_name,
      options.output_score_tensor_index);
  if (scores == nullptr) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("No output score tensor found with name ",
                     options.output_score_tensor_name, " or at index ",
                     options.output_score_tensor_index),
        TfLiteSupportStatus::kOutputTensorNotFoundError);
  }
  static constexpr TfLiteType valid_types[] = {kTfLiteUInt8,   kTfLiteInt8,
                                               kTfLiteInt16,   kTfLiteFloat32,
                                               kTfLiteFloat64, kTfLiteBool};
  if (!absl::c_linear_search(valid_types, scores->type)) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrCat("Type mismatch for score tensor ", scores->name,
                     ". Requested one of these types: "
                     "INT8/UINT8/INT16/FLOAT32/FLOAT64/BOOL, got ",
                     TfLiteTypeGetName(scores->type), "."),
        TfLiteSupportStatus::kInvalidOutputTensorTypeError);
  }

  // Extract associated label file from output score tensor if one exists, a
  // well-formatted metadata should have same number of tensors with the model.
  if (output_tensor_metadatas &&
      output_tensor_metadatas->size() == output_tensors.size()) {
    for (int i = 0; i < output_tensor_metadatas->size(); ++i) {
      const tflite::TensorMetadata* metadata = output_tensor_metadatas->Get(i);
      if ((metadata->name() && metadata->name()->string_view() ==
                                   options.output_score_tensor_name) ||
          i == options.output_score_tensor_index) {
        if (TrySetLabelFromMetadata(metadata).ok()) {
          return absl::OkStatus();
        }
      }
    }
  }

  // If labels_vector_ is not set up from metadata, try register output label
  // tensor from options.
  if (labels_vector_ == nullptr) {
    // output label tensor should be type STRING or INT32 if the one exists
    auto labels = FindTensorWithNameOrIndex(
        output_tensors, output_tensor_metadatas,
        options.output_label_tensor_name, options.output_label_tensor_index);
    if (labels != nullptr && labels->type != kTfLiteString &&
        labels->type != kTfLiteInt32) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrCat("Type mismatch for label tensor ", scores->name,
                       ". Requested STRING or INT32, got ",
                       TfLiteTypeGetName(scores->type), "."),
          TfLiteSupportStatus::kInvalidOutputTensorTypeError);
    }
  }
  return absl::OkStatus();
}

StatusOr<std::unique_ptr<NLClassifier>>
NLClassifier::CreateFromBufferAndOptions(
    const char* model_buffer_data,
    size_t model_buffer_size,
    const NLClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<NLClassifier> nl_classifier;
  ASSIGN_OR_RETURN(
      nl_classifier,
      core::TaskAPIFactory::CreateFromBuffer<NLClassifier>(
          model_buffer_data, model_buffer_size, std::move(resolver)));
  RETURN_IF_ERROR(nl_classifier->Initialize(options));
  return std::move(nl_classifier);
}

StatusOr<std::unique_ptr<NLClassifier>> NLClassifier::CreateFromFileAndOptions(
    const std::string& path_to_model,
    const NLClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<NLClassifier> nl_classifier;
  ASSIGN_OR_RETURN(nl_classifier,
                   core::TaskAPIFactory::CreateFromFile<NLClassifier>(
                       path_to_model, std::move(resolver)));
  RETURN_IF_ERROR(nl_classifier->Initialize(options));
  return std::move(nl_classifier);
}

StatusOr<std::unique_ptr<NLClassifier>> NLClassifier::CreateFromFdAndOptions(
    int fd,
    const NLClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  std::unique_ptr<NLClassifier> nl_classifier;
  ASSIGN_OR_RETURN(nl_classifier,
                   core::TaskAPIFactory::CreateFromFileDescriptor<NLClassifier>(
                       fd, std::move(resolver)));
  RETURN_IF_ERROR(nl_classifier->Initialize(options));
  return std::move(nl_classifier);
}

bool NLClassifier::HasRegexTokenizerMetadata() {
  const TensorMetadata* input_tensor_metadata =
      GetMetadataExtractor()->GetInputTensorMetadata(
          kRegexTokenizerInputTensorIndex);
  if (input_tensor_metadata == nullptr) {
    return false;
  }
  tflite::support::StatusOr<const tflite::ProcessUnit*> status =
      GetMetadataExtractor()->FindFirstProcessUnit(
          *input_tensor_metadata, ProcessUnitOptions_RegexTokenizerOptions);
  return status.ok() ? status.value() != nullptr : false;
}

absl::Status NLClassifier::SetupRegexTokenizer() {
  ASSIGN_OR_RETURN(
      std::unique_ptr<Tokenizer> base_tokenizer,
      CreateRegexTokenizerFromProcessUnit(
          GetMetadataExtractor()
              ->GetInputTensorMetadata(kRegexTokenizerInputTensorIndex)
              ->process_units()
              ->Get(kRegexTokenizerProcessUnitIndex),
          GetMetadataExtractor()));

  tokenizer_ = std::unique_ptr<RegexTokenizer>(
      dynamic_cast<RegexTokenizer*>(base_tokenizer.release()));

  return absl::OkStatus();
}

}  // namespace nlclassifier
}  // namespace text
}  // namespace task
}  // namespace tflite
