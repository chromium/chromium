/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/task/text/bert_clu_annotator.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/task_api_factory.h"
#include "tensorflow_lite_support/cc/task/core/task_utils.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/tflite_modules.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"

namespace tflite {
namespace task {
namespace text {
namespace clu {

namespace {
constexpr int kTokenizerProcessUnitIndex = 0;

constexpr char kTokenIdTensorName[] = "ids";
constexpr char kMaskTensorName[] = "mask";
constexpr char kTokenTypeIdTensorName[] = "segment_ids";

constexpr char kDomainTaskNamesTensorName[] = "domain_task/names";
constexpr char kDomainTaskScoresTensorName[] = "domain_task/scores";
constexpr char kIntentTaskNamesTensorName[] = "intent_task/names";
constexpr char kIntentTaskScoresTensorName[] = "intent_task/scores";
constexpr char kSlotTaskNamesTensorName[] = "slot_task/names";
constexpr char kSlotTaskScoresTensorName[] = "slot_task/scores";

absl::Status SanityCheckOptions(const BertCluAnnotatorOptions& options) {
  if (!options.has_base_options()) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Missing mandatory `base_options` field",
        support::TfLiteSupportStatus::kInvalidArgumentError);
  }
  return absl::OkStatus();
}

absl::StatusOr<int> FindTensorIdxByName(
    const flatbuffers::Vector<flatbuffers::Offset<TensorMetadata>>*
        tensor_metadatas,
    const std::string& name) {
  int tensor_idx = core::FindTensorIndexByMetadataName(tensor_metadatas, name);
  if (tensor_idx == -1) {
    return absl::InternalError(absl::StrFormat(
        "The expected tensor name \"%s\" is not found in metadata list.",
        name));
  }
  return tensor_idx;
}
}  // namespace

/*static*/ tflite::support::StatusOr<std::unique_ptr<CluAnnotator>>
BertCluAnnotator::CreateFromOptions(
    const BertCluAnnotatorOptions& options,
    std::unique_ptr<tflite::OpResolver> resolver) {
  TFLITE_RETURN_IF_ERROR(SanityCheckOptions(options));

  // Copy options to ensure the ExternalFile outlives the duration of this
  // created BertCluAnnotator object.
  auto options_copy = std::make_unique<BertCluAnnotatorOptions>(options);

  TFLITE_ASSIGN_OR_RETURN(
      auto bert_clu_annotator,
      core::TaskAPIFactory::CreateFromBaseOptions<BertCluAnnotator>(
          &options_copy->base_options(), std::move(resolver)));
  TFLITE_RETURN_IF_ERROR(bert_clu_annotator->Init(std::move(options_copy)));
  return std::move(bert_clu_annotator);
}

absl::Status BertCluAnnotator::Init(
    std::unique_ptr<BertCluAnnotatorOptions> options) {
  options_ = std::move(options);

  const ProcessUnit* tokenizer_process_unit =
      GetMetadataExtractor()->GetInputProcessUnit(kTokenizerProcessUnitIndex);
  if (tokenizer_process_unit == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No input process unit found from metadata.",
        support::TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  TFLITE_ASSIGN_OR_RETURN(tokenizer_,
                   support::text::tokenizer::CreateTokenizerFromProcessUnit(
                       tokenizer_process_unit, GetMetadataExtractor()));

  // Initialize the modules.
  auto* interpreter = GetTfLiteEngine()->interpreter();

  const metadata::ModelMetadataExtractor* metadata_extractor =
      GetTfLiteEngine()->metadata_extractor();
  auto input_tensors_metadata = metadata_extractor->GetInputTensorMetadata();
  auto output_tensors_metadata = metadata_extractor->GetOutputTensorMetadata();

  tensor_index_map_ = std::make_unique<TensorIndexMap>();

  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->token_id_idx,
      FindTensorIdxByName(input_tensors_metadata, kTokenIdTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->token_mask_idx,
      FindTensorIdxByName(input_tensors_metadata, kMaskTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->token_type_id_idx,
      FindTensorIdxByName(input_tensors_metadata, kTokenTypeIdTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->domain_names_idx,
      FindTensorIdxByName(output_tensors_metadata, kDomainTaskNamesTensorName));
  TFLITE_ASSIGN_OR_RETURN(tensor_index_map_->domain_scores_idx,
                   FindTensorIdxByName(output_tensors_metadata,
                                       kDomainTaskScoresTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->intent_names_idx,
      FindTensorIdxByName(output_tensors_metadata, kIntentTaskNamesTensorName));
  TFLITE_ASSIGN_OR_RETURN(tensor_index_map_->intent_scores_idx,
                   FindTensorIdxByName(output_tensors_metadata,
                                       kIntentTaskScoresTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->slot_names_idx,
      FindTensorIdxByName(output_tensors_metadata, kSlotTaskNamesTensorName));
  TFLITE_ASSIGN_OR_RETURN(
      tensor_index_map_->slot_scores_idx,
      FindTensorIdxByName(output_tensors_metadata, kSlotTaskScoresTensorName));

  absl::StatusOr<std::unique_ptr<AbstractModule>> m;
  // UtteranceSeqModule
  m = UtteranceSeqModule::Create(
      interpreter, tensor_index_map_.get(), options_.get(),
      static_cast<tflite::support::text::tokenizer::BertTokenizer*>(
          tokenizer_.get()));
  TFLITE_RETURN_IF_ERROR(m.status());
  modules_.emplace_back(*std::move(m));
  // DomainModule.
  m = DomainModule::Create(interpreter, tensor_index_map_.get(),
                           options_.get());
  TFLITE_RETURN_IF_ERROR(m.status());
  modules_.emplace_back(*std::move(m));
  // IntentModule.
  m = IntentModule::Create(interpreter, tensor_index_map_.get(),
                           options_.get());
  TFLITE_RETURN_IF_ERROR(m.status());
  modules_.emplace_back(*std::move(m));
  // SlotModule.
  m = SlotModule::Create(interpreter, tensor_index_map_.get(), options_.get());
  TFLITE_RETURN_IF_ERROR(m.status());
  modules_.emplace_back(*std::move(m));

  return absl::OkStatus();
}

absl::StatusOr<CluResponse> BertCluAnnotator::Annotate(
    const CluRequest& request) {
  return Infer(request);
}

absl::Status BertCluAnnotator::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const CluRequest& request) {
  artifacts_.Clear();
  // Preprocess
  for (const auto& module : modules_) {
    TFLITE_RETURN_IF_ERROR(module->Preprocess(request, &artifacts_));
  }
  return absl::OkStatus();
}

tflite::support::StatusOr<CluResponse> BertCluAnnotator::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors,
    const CluRequest& request) {
  CluResponse response;
  for (const auto& module : modules_) {
    TFLITE_RETURN_IF_ERROR(module->Postprocess(&artifacts_, &response));
  }
  return response;
}

}  // namespace clu
}  // namespace text
}  // namespace task
}  // namespace tflite
