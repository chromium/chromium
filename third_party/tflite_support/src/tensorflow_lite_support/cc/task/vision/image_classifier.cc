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

#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"

#include "absl/algorithm/container.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/integral_types.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/core/tflite_engine.h"
#include "tensorflow_lite_support/cc/task/vision/core/label_map_item.h"
#include "tensorflow_lite_support/cc/task/vision/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_utils.h"
#include "tensorflow_lite_support/metadata/cc/metadata_extractor.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace task {
namespace vision {

namespace {

using ::absl::StatusCode;
using ::tflite::metadata::ModelMetadataExtractor;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;
using ::tflite::task::core::AssertAndReturnTypedTensor;
using ::tflite::task::core::TaskAPIFactory;
using ::tflite::task::core::TfLiteEngine;

}  // namespace

/* static */
StatusOr<std::unique_ptr<ImageClassifier>> ImageClassifier::CreateFromOptions(
    const ImageClassifierOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the constructed object.
  auto options_copy = absl::make_unique<ImageClassifierOptions>(options);

  std::unique_ptr<ImageClassifier> image_classifier;
  if (options_copy->has_model_file_with_metadata()) {
    TFLITE_ASSIGN_OR_RETURN(
        image_classifier,
        TaskAPIFactory::CreateFromExternalFileProto<ImageClassifier>(
            &options_copy->model_file_with_metadata(), std::move(resolver),
            options_copy->num_threads(), options_copy->compute_settings()));
  } else if (options_copy->base_options().has_model_file()) {
    TFLITE_ASSIGN_OR_RETURN(image_classifier,
                     TaskAPIFactory::CreateFromBaseOptions<ImageClassifier>(
                         &options_copy->base_options(), std::move(resolver)));
  } else {
    // Should never happen because of SanityCheckOptions.
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found 0."),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  TFLITE_RETURN_IF_ERROR(image_classifier->Init(std::move(options_copy)));

  return image_classifier;
}

/* static */
absl::Status ImageClassifier::SanityCheckOptions(
    const ImageClassifierOptions& options) {
  int num_input_models = (options.base_options().has_model_file() ? 1 : 0) +
                         (options.has_model_file_with_metadata() ? 1 : 0);
  if (num_input_models != 1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Expected exactly one of `base_options.model_file` or "
                        "`model_file_with_metadata` to be provided, found %d.",
                        num_input_models),
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.max_results() == 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "Invalid `max_results` option: value must be != 0",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.class_name_whitelist_size() > 0 &&
      options.class_name_blacklist_size() > 0) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "`class_name_whitelist` and `class_name_blacklist` are mutually "
        "exclusive options.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  if (options.num_threads() == 0 || options.num_threads() < -1) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        "`num_threads` must be greater than 0 or equal to -1.",
        TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

absl::Status ImageClassifier::Init(
    std::unique_ptr<ImageClassifierOptions> options) {
  // Set options.
  options_ = std::move(options);

  // Perform pre-initialization actions (by default, sets the process engine for
  // image pre-processing to kLibyuv as a sane default).
  TFLITE_RETURN_IF_ERROR(PreInit());

  // Sanity check and set inputs and outputs.
  TFLITE_RETURN_IF_ERROR(CheckAndSetInputs());
  TFLITE_RETURN_IF_ERROR(CheckAndSetOutputs());

  // Initialize class whitelisting/blacklisting, if any.
  TFLITE_RETURN_IF_ERROR(CheckAndSetClassNameSet());

  // Perform final initialization (by default, initialize score calibration
  // parameters, if any).
  TFLITE_RETURN_IF_ERROR(PostInit());

  return absl::OkStatus();
}

absl::Status ImageClassifier::PreInit() {
  SetProcessEngine(FrameBufferUtils::ProcessEngine::kLibyuv);
  return absl::OkStatus();
}

absl::Status ImageClassifier::PostInit() { return InitScoreCalibrations(); }

absl::Status ImageClassifier::CheckAndSetOutputs() {
  num_outputs_ = TfLiteEngine::OutputCount(GetTfLiteEngine()->interpreter());

  // Perform sanity checks and extract metadata.
  const ModelMetadataExtractor* metadata_extractor =
      GetTfLiteEngine()->metadata_extractor();

  const flatbuffers::Vector<flatbuffers::Offset<tflite::TensorMetadata>>*
      output_tensor_metadata = metadata_extractor->GetOutputTensorMetadata();

  // Loop over output tensors metadata, if any.
  // Note: models with no output tensor metadata at all are supported.
  if (output_tensor_metadata != nullptr) {
    int num_output_tensors = output_tensor_metadata->size();

    if (num_outputs_ != num_output_tensors) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Mismatch between number of output tensors (%d) and "
                          "output tensors "
                          "metadata (%d).",
                          num_outputs_, num_output_tensors),
          TfLiteSupportStatus::kMetadataInconsistencyError);
    }

    for (int i = 0; i < num_output_tensors; ++i) {
      const tflite::TensorMetadata* output_tensor =
          output_tensor_metadata->Get(i);

      TFLITE_ASSIGN_OR_RETURN(
          ClassificationHead head,
          BuildClassificationHead(*metadata_extractor, *output_tensor,
                                  options_->display_names_locale()));

      classification_heads_.emplace_back(std::move(head));
    }
  }

  // If classifier heads are not set, build default ones based on model
  // introspection. This happens if a model with partial or no metadata was
  // provided through the `model_file_with_metadata` options field.
  if (classification_heads_.empty()) {
    classification_heads_.reserve(num_outputs_);
    for (int output_index = 0; output_index < num_outputs_; ++output_index) {
      classification_heads_.emplace_back(ClassificationHead{});
    }
  }

  if (num_outputs_ != classification_heads_.size()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Got %d classifier head(s), expected %d according to "
                        "the label map.",
                        num_outputs_, classification_heads_.size()),
        TfLiteSupportStatus::kMetadataInconsistencyError);
  }

  int num_quantized_outputs = 0;
  for (int i = 0; i < num_outputs_; ++i) {
    const TfLiteTensor* output_tensor =
        TfLiteEngine::GetOutput(GetTfLiteEngine()->interpreter(), i);
    const int num_dimensions = output_tensor->dims->size;
    if (num_dimensions == 4) {
      if (output_tensor->dims->data[1] != 1 ||
          output_tensor->dims->data[2] != 1) {
        return CreateStatusWithPayload(
            StatusCode::kInvalidArgument,
            absl::StrFormat("Unexpected WxH sizes for output index %d: got "
                            "%dx%d, expected 1x1.",
                            i, output_tensor->dims->data[2],
                            output_tensor->dims->data[1]),
            TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
      }
    } else if (num_dimensions != 2) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "Unexpected number of dimensions for output index %d: got %dD, "
              "expected either 2D (BxN with B=1) or 4D (BxHxWxN with B=1, W=1, "
              "H=1).",
              i, num_dimensions),
          TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
    }
    if (output_tensor->dims->data[0] != 1) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("The output array is expected to have a batch size "
                          "of 1. Got %d for output index %d.",
                          output_tensor->dims->data[0], i),
          TfLiteSupportStatus::kInvalidOutputTensorDimensionsError);
    }
    int num_classes = output_tensor->dims->data[num_dimensions - 1];
    // If label map is not set, build a default one based on model
    // introspection. This happens if a model with partial or no metadata was
    // provided through the `model_file_with_metadata` options field.
    if (classification_heads_[i].label_map_items.empty()) {
      classification_heads_[i].label_map_items.reserve(num_classes);
      for (int class_index = 0; class_index < num_classes; ++class_index) {
        classification_heads_[i].label_map_items.emplace_back(LabelMapItem{});
      }
    }
    int num_label_map_items = classification_heads_[i].label_map_items.size();
    if (num_classes != num_label_map_items) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Got %d class(es) for output index %d, expected %d "
                          "according to the label map.",
                          output_tensor->dims->data[num_dimensions - 1], i,
                          num_label_map_items),
          TfLiteSupportStatus::kMetadataInconsistencyError);
    }
    if (output_tensor->type == kTfLiteUInt8) {
      num_quantized_outputs++;
    } else if (output_tensor->type != kTfLiteFloat32) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Type mismatch for output tensor %s. Requested one "
                          "of these types: "
                          "kTfLiteUint8/kTfLiteFloat32, got %s.",
                          output_tensor->name,
                          TfLiteTypeGetName(output_tensor->type)),
          TfLiteSupportStatus::kInvalidOutputTensorTypeError);
    }
  }

  if (num_quantized_outputs > 0 && num_quantized_outputs != num_outputs_) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat("Got %d quantized output(s), expected %d (i.e. all "
                        "provided outputs must be quantized).",
                        num_quantized_outputs, num_outputs_),
        TfLiteSupportStatus::kInvalidOutputTensorTypeError);
  }
  has_uint8_outputs_ = (num_quantized_outputs > 0);

  return absl::OkStatus();
}

absl::Status ImageClassifier::CheckAndSetClassNameSet() {
  // Exit early if no blacklist/whitelist.
  if (options_->class_name_blacklist_size() == 0 &&
      options_->class_name_whitelist_size() == 0) {
    return absl::OkStatus();
  }

  // Before processing class names whitelist or blacklist from the input options
  // create a set with _all_ known class names from the label map(s).
  absl::flat_hash_set<std::string> all_class_names;
  int head_index = 0;
  for (const auto& head : classification_heads_) {
    absl::flat_hash_set<std::string> head_class_names;
    for (const auto& item : head.label_map_items) {
      if (!item.name.empty()) {
        head_class_names.insert(item.name);
      }
    }
    if (head_class_names.empty()) {
      std::string name = head.name;
      if (name.empty()) {
        name = absl::StrFormat("#%d", head_index);
      }
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat(
              "Using `class_name_whitelist` or `class_name_blacklist` "
              "requires labels to be present but none was found for "
              "classification head: %s",
              name),
          TfLiteSupportStatus::kMetadataMissingLabelsError);
    }
    all_class_names.insert(head_class_names.begin(), head_class_names.end());
    head_index++;
  }

  class_name_set_.is_whitelist = options_->class_name_whitelist_size() > 0;
  const auto& class_names = class_name_set_.is_whitelist
                                ? options_->class_name_whitelist()
                                : options_->class_name_blacklist();

  // Note: duplicate or unknown classes are just ignored.
  class_name_set_.values.clear();
  for (const auto& class_name : class_names) {
    if (!all_class_names.contains(class_name)) {
      continue;
    }
    class_name_set_.values.insert(class_name);
  }

  if (class_name_set_.values.empty()) {
    return CreateStatusWithPayload(
        StatusCode::kInvalidArgument,
        absl::StrFormat(
            "Invalid class names specified via `class_name_%s`: none match "
            "with model labels.",
            class_name_set_.is_whitelist ? "whitelist" : "blacklist"),
        TfLiteSupportStatus::kInvalidArgumentError);
  }

  return absl::OkStatus();
}

absl::Status ImageClassifier::InitScoreCalibrations() {
  score_calibrations_.clear();
  score_calibrations_.resize(classification_heads_.size());

  for (int i = 0; i < classification_heads_.size(); ++i) {
    if (!classification_heads_[i].calibration_params.has_value()) {
      continue;
    }

    score_calibrations_[i] = absl::make_unique<ScoreCalibration>();
    if (score_calibrations_[i] == nullptr) {
      return CreateStatusWithPayload(
          StatusCode::kInternal, "Could not create score calibration object.");
    }

    TFLITE_RETURN_IF_ERROR(score_calibrations_[i]->InitializeFromParameters(
        classification_heads_[i].calibration_params.value()));
  }

  return absl::OkStatus();
}

StatusOr<ClassificationResult> ImageClassifier::Classify(
    const FrameBuffer& frame_buffer) {
  BoundingBox roi;
  roi.set_width(frame_buffer.dimension().width);
  roi.set_height(frame_buffer.dimension().height);
  return Classify(frame_buffer, roi);
}

StatusOr<ClassificationResult> ImageClassifier::Classify(
    const FrameBuffer& frame_buffer, const BoundingBox& roi) {
  return InferWithFallback(frame_buffer, roi);
}

StatusOr<ClassificationResult> ImageClassifier::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const FrameBuffer& /*frame_buffer*/, const BoundingBox& /*roi*/) {
  if (output_tensors.size() != num_outputs_) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Expected %d output tensors, found %d", num_outputs_,
                        output_tensors.size()));
  }

  ClassificationResult result;
  std::vector<std::pair<int, float>> score_pairs;

  for (int i = 0; i < num_outputs_; ++i) {
    auto* classifications = result.add_classifications();
    classifications->set_head_index(i);

    const auto& head = classification_heads_[i];
    score_pairs.clear();
    score_pairs.reserve(head.label_map_items.size());

    const TfLiteTensor* output_tensor = output_tensors[i];
    if (has_uint8_outputs_) {
      TFLITE_ASSIGN_OR_RETURN(const uint8* output_data,
                       AssertAndReturnTypedTensor<uint8>(output_tensor));
      for (int j = 0; j < head.label_map_items.size(); ++j) {
        score_pairs.emplace_back(j, output_tensor->params.scale *
                                        (static_cast<int>(output_data[j]) -
                                         output_tensor->params.zero_point));
      }
    } else {
      TFLITE_ASSIGN_OR_RETURN(const float* output_data,
                       AssertAndReturnTypedTensor<float>(output_tensor));
      for (int j = 0; j < head.label_map_items.size(); ++j) {
        score_pairs.emplace_back(j, output_data[j]);
      }
    }

    // Optional score calibration.
    if (score_calibrations_[i] != nullptr) {
      for (auto& score_pair : score_pairs) {
        const std::string& class_name =
            head.label_map_items[score_pair.first].name;

        // In ComputeCalibratedScore, score_pair.second is set to the
        // default_score value from metadata [1] if the category (1) has no
        // score calibration data or (2) has a very low confident uncalibrated
        // score, i.e. lower than the `min_uncalibrated_score` threshold.
        // Otherwise, score_pair.second is calculated based on the selected
        // score transformation function, and the value is guaranteed to be in
        // the range of [0, scale], where scale is a label-dependent sigmoid
        // parameter.
        //
        // [1]:
        // https://github.com/tensorflow/tflite-support/blob/af26cb6952ccdeee0e849df2b93dbe7e57f6bc48/tensorflow_lite_support/metadata/metadata_schema.fbs#L453
        score_pair.second = score_calibrations_[i]->ComputeCalibratedScore(
            class_name, score_pair.second);
      }
    }

    int num_results =
        options_->max_results() >= 0
            ? std::min(static_cast<int>(head.label_map_items.size()),
                       options_->max_results())
            : head.label_map_items.size();
    float score_threshold = options_->has_score_threshold()
                                ? options_->score_threshold()
                                : head.score_threshold;

    if (class_name_set_.values.empty()) {
      // Partially sort in descending order (higher score is better).
      absl::c_partial_sort(
          score_pairs, score_pairs.begin() + num_results,
          [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
            return a.second > b.second;
          });

      for (int j = 0; j < num_results; ++j) {
        float score = score_pairs[j].second;
        if (score < score_threshold) {
          break;
        }
        auto* cl = classifications->add_classes();
        cl->set_index(score_pairs[j].first);
        cl->set_score(score);
      }
    } else {
      // Sort in descending order (higher score is better).
      absl::c_sort(score_pairs, [](const std::pair<int, float>& a,
                                   const std::pair<int, float>& b) {
        return a.second > b.second;
      });

      for (int j = 0; j < head.label_map_items.size(); ++j) {
        float score = score_pairs[j].second;
        if (score < score_threshold ||
            classifications->classes_size() >= num_results) {
          break;
        }

        const int class_index = score_pairs[j].first;
        const std::string& class_name = head.label_map_items[class_index].name;

        bool class_name_found = class_name_set_.values.contains(class_name);

        if ((!class_name_found && class_name_set_.is_whitelist) ||
            (class_name_found && !class_name_set_.is_whitelist)) {
          continue;
        }

        auto* cl = classifications->add_classes();
        cl->set_index(class_index);
        cl->set_score(score);
      }
    }
  }

  TFLITE_RETURN_IF_ERROR(FillResultsFromLabelMaps(&result));

  return result;
}

absl::Status ImageClassifier::FillResultsFromLabelMaps(
    ClassificationResult* result) {
  for (int i = 0; i < result->classifications_size(); ++i) {
    Classifications* classifications = result->mutable_classifications(i);
    int head_index = classifications->head_index();
    if (head_index < 0 || head_index >= classification_heads_.size()) {
      return CreateStatusWithPayload(
          StatusCode::kInvalidArgument,
          absl::StrFormat("Invalid head index (%d) with respect to total "
                          "number of classification heads (%d).",
                          head_index, classification_heads_.size()),
          TfLiteSupportStatus::kMetadataInconsistencyError);
    }
    const std::vector<LabelMapItem>& label_map_items =
        classification_heads_[head_index].label_map_items;
    for (int j = 0; j < classifications->classes_size(); ++j) {
      Class* current_class = classifications->mutable_classes(j);
      int current_class_index = current_class->index();
      if (current_class_index < 0 ||
          current_class_index >= label_map_items.size()) {
        return CreateStatusWithPayload(
            StatusCode::kInvalidArgument,
            absl::StrFormat("Invalid class index (%d) with respect to label "
                            "map size (%d) for head #%d.",
                            current_class_index, label_map_items.size(),
                            head_index),
            TfLiteSupportStatus::kMetadataInconsistencyError);
      }
      const std::string& name = label_map_items[current_class_index].name;
      if (!name.empty()) {
        current_class->set_class_name(name);
      }
      const std::string& display_name =
          label_map_items[current_class_index].display_name;
      if (!display_name.empty()) {
        current_class->set_display_name(display_name);
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
