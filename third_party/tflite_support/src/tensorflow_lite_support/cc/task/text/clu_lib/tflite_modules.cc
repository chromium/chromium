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

#include "tensorflow_lite_support/cc/task/text/clu_lib/tflite_modules.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/status/statusor.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/str_join.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/core/proto/class.pb.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/bert_utils.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/constants.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/intent_repr.h"
#include "tensorflow_lite_support/cc/task/text/clu_lib/slot_tagging_output.h"
#include "tensorflow_lite_support/cc/task/text/proto/bert_clu_annotator_options_proto_inc.h"

namespace tflite::task::text::clu {
// This handles WordPiece tokenizer for BERT-DeepCLU. It populates the input
// tensors by concatenating the current utterance with history turns. It also
// sets utterance_turn_id_seq for post-processing.
absl::Status PopulateInputTextTensorForBERT(
    const CluRequest& request, int token_id_tensor_idx,
    int token_mask_tensor_idx, int token_type_id_tensor_idx,
    const tflite::support::text::tokenizer::BertTokenizer* tokenizer,
    size_t max_seq_len, int max_history_turns,
    core::TfLiteEngine::Interpreter* interpreter, Artifacts* artifacts) {
  size_t seq_len;
  int64_t* tokens_tensor =
      interpreter->typed_input_tensor<int64_t>(token_id_tensor_idx);
  if (tokens_tensor == nullptr) {
    return absl::InternalError("tokens_tensor is nullptr");
  }
  const int turns_to_encode =
      std::min(max_history_turns + 1, request.utterances_size());
  // Prepare the utterance list of the current turn and the history.
  std::vector<absl::string_view> utterance_list;
  artifacts->reverse_utterance_list_to_encode.reserve(turns_to_encode);
  for (int reverse_turn_id = 0; reverse_turn_id < turns_to_encode;
       ++reverse_turn_id) {
    artifacts->reverse_utterance_list_to_encode.emplace_back(
        request.utterances(request.utterances_size() - 1 - reverse_turn_id));
  }
  // Call BERT preprocessing.
  std::vector<int> token_ids;
  std::vector<std::pair<int, int>> alignments;
  std::vector<int> first_subword_indicators;
  std::vector<int> segment_id_list;
  TFLITE_RETURN_IF_ERROR(BertPreprocessing(
      tokenizer, artifacts->reverse_utterance_list_to_encode, max_seq_len,
      max_history_turns, &token_ids, &alignments, &first_subword_indicators,
      &segment_id_list, &(artifacts->token_turn_ids)));
  // Populate tokens.
  for (int i = 0; i < token_ids.size(); ++i) {
    tokens_tensor[i] = token_ids[i];
  }
  seq_len = token_ids.size();
  // Pad the remaining.
  int pad_int;
  if (!tokenizer->LookupId(kWordpiecePadToken, &pad_int)) {
    return absl::InternalError(
        absl::StrCat("Cannot locate id for ", kWordpiecePadToken));
  }
  for (int i = seq_len; i < max_seq_len; ++i) {
    tokens_tensor[i] = pad_int;
  }
  // Token alignments.
  artifacts->token_alignments = std::move(alignments);
  // Token first subword indicators.
  artifacts->first_subword_indicators = std::move(first_subword_indicators);
  // Populate segment_id.
  int64_t* segment_ids_tensor =
      interpreter->typed_input_tensor<int64_t>(token_type_id_tensor_idx);
  if (segment_ids_tensor == nullptr) {
    return absl::InternalError("segment_ids_tensor is nullptr");
  }
  for (int i = 0; i < segment_id_list.size(); ++i) {
    segment_ids_tensor[i] = segment_id_list[i];
  }
  // Pad the remaining.
  for (int i = seq_len; i < max_seq_len; ++i) {
    segment_ids_tensor[i] = 0;  // Always.
  }

  // Token alignment does not need to be padded as it's not used in the TF
  // graph. It's instead used in populating the response

  // Populate the input mask.
  int32_t* masks_tensor =
      interpreter->typed_input_tensor<int32_t>(token_mask_tensor_idx);
  if (masks_tensor == nullptr) {
    return absl::InternalError("masks_tensor is nullptr");
  }
  for (int i = 0; i < max_seq_len; ++i) {
    masks_tensor[i] = i < seq_len ? 1 : 0;
  }
  return absl::OkStatus();
}

absl::StatusOr<int> GetInputSeqDimSize(
    const size_t input_idx,
    const core::TfLiteEngine::Interpreter* interpreter) {
  if (input_idx >= interpreter->inputs().size()) {
    return absl::InternalError(absl::StrCat(
        "input_idx should be less than interpreter input numbers. ", input_idx,
        " v.s. ", interpreter->inputs().size()));
  }
  const auto tensor = interpreter->input_tensor(input_idx);
  if (tflite::NumDimensions(tensor) < 2) {
    return absl::InternalError(absl::StrCat(
        "the dimension of the input tensor should be less than 2; found ",
        tflite::NumDimensions(tensor)));
  }
  return tflite::SizeOfDimension(tensor, 1);
}

absl::Status AbstractModule::Init(core::TfLiteEngine::Interpreter* interpreter,
                                  const BertCluAnnotatorOptions* options) {
  interpreter_ = interpreter;
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<AbstractModule>> UtteranceSeqModule::Create(
    core::TfLiteEngine::Interpreter* interpreter,
    const TensorIndexMap* tensor_index_map,
    const BertCluAnnotatorOptions* options,
    const tflite::support::text::tokenizer::BertTokenizer* tokenizer) {
  auto out = std::make_unique<UtteranceSeqModule>();
  out->tensor_index_map_ = tensor_index_map;
  TFLITE_RETURN_IF_ERROR(out->Init(interpreter, options));
  out->tokenizer_ = tokenizer;
  TFLITE_ASSIGN_OR_RETURN(
      out->max_seq_len_,
      GetInputSeqDimSize(tensor_index_map->token_id_idx, interpreter));
  out->max_history_turns_ = options->max_history_turns();
  return out;
}

absl::Status UtteranceSeqModule::Preprocess(const CluRequest& request,
                                            Artifacts* artifacts) const {
  return PopulateInputTextTensorForBERT(
      request, tensor_index_map_->token_id_idx,
      tensor_index_map_->token_mask_idx, tensor_index_map_->token_type_id_idx,
      tokenizer_, max_seq_len_, max_history_turns_, interpreter_, artifacts);
}

/////////////////////////////// Output tasks

absl::StatusOr<AbstractModule::NamesAndConfidences>
AbstractModule::NamesAndConfidencesFromOutput(int names_tensor_idx,
                                              int scores_tensor_idx) const {
  const TfLiteTensor* names_data =
      interpreter_->output_tensor(names_tensor_idx);
  const size_t size = tflite::GetStringCount(names_data);
  const float* confidence_data =
      interpreter_->typed_output_tensor<float>(scores_tensor_idx);
  if (size > tflite::SizeOfDimension(
                 interpreter_->output_tensor(scores_tensor_idx), 1)) {
    return absl::InternalError("");
  }
  NamesAndConfidences ret;
  auto& [names, confidences] = ret;
  confidences = confidence_data;
  names.reserve(size);
  for (int i = 0; i < size; ++i) {
    const auto ref = tflite::GetString(names_data, i);
    names.emplace_back(absl::string_view(ref.str, ref.len));
  }
  return ret;
}

absl::StatusOr<std::unique_ptr<AbstractModule>> DomainModule::Create(
    core::TfLiteEngine::Interpreter* interpreter,
    const TensorIndexMap* tensor_index_map,
    const BertCluAnnotatorOptions* options) {
  auto out = std::make_unique<DomainModule>();
  out->tensor_index_map_ = tensor_index_map;
  out->domain_threshold_ = options->domain_threshold();
  TFLITE_RETURN_IF_ERROR(out->Init(interpreter, options));
  return out;
}

absl::Status DomainModule::Postprocess(Artifacts* artifacts,
                                       CluResponse* response) const {
  TFLITE_ASSIGN_OR_RETURN(
      const auto t_output,
      NamesAndConfidencesFromOutput(tensor_index_map_->domain_names_idx,
                                    tensor_index_map_->domain_scores_idx));
  const auto& [names, confidences] = t_output;
  for (int i = 0; i < names.size(); ++i) {
    if (confidences[i] < domain_threshold_) continue;
    auto domain = response->add_domains();
    // Conversion to string is needed due to portable_proto generated code
    const std::string names_i(names[i]);
    domain->set_display_name(names_i);
    domain->set_score(confidences[i]);
  }
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<AbstractModule>> IntentModule::Create(
    core::TfLiteEngine::Interpreter* interpreter,
    const TensorIndexMap* tensor_index_map,
    const BertCluAnnotatorOptions* options) {
  auto out = std::make_unique<IntentModule>();
  out->tensor_index_map_ = tensor_index_map;
  out->intent_threshold_ = options->intent_threshold();
  out->categorical_slot_threshold_ = options->categorical_slot_threshold();
  TFLITE_RETURN_IF_ERROR(out->Init(interpreter, options));
  return out;
}

absl::Status IntentModule::Postprocess(Artifacts* artifacts,
                                       CluResponse* response) const {
  TFLITE_ASSIGN_OR_RETURN(
      const auto t_output,
      NamesAndConfidencesFromOutput(tensor_index_map_->intent_names_idx,
                                    tensor_index_map_->intent_scores_idx));
  const auto& [names, confidences] = t_output;

  for (int i = 0; i < names.size(); ++i) {
    TFLITE_ASSIGN_OR_RETURN(const auto name, IntentRepr::CreateFromFullName(names[i]));
    // TODO(xysong): Differentiate categorical slots from intents.
    std::vector<absl::string_view> parts = absl::StrSplit(name.Name(), '=');
    if (parts.size() == 2) {
      // The name is like 'xxx=yyy'. It's a categorical slot.
      if (confidences[i] < categorical_slot_threshold_) continue;
      auto new_categorical_slot = response->mutable_categorical_slots()->Add();

      const auto slot = std::string(parts[0]);
      new_categorical_slot->set_slot(slot);
      auto new_categorical_slot_prediction =
          new_categorical_slot->mutable_prediction();
      const auto display_name = std::string(parts[1]);
      new_categorical_slot_prediction->set_display_name(display_name);
      new_categorical_slot_prediction->set_score(confidences[i]);
    } else {
      // It's an intent.
      if (confidences[i] < intent_threshold_) continue;
      auto new_intent = response->mutable_intents()->Add();
      new_intent->set_display_name(name.Name());
      new_intent->set_score(confidences[i]);
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<AbstractModule>> SlotModule::Create(
    core::TfLiteEngine::Interpreter* interpreter,
    const TensorIndexMap* tensor_index_map,
    const BertCluAnnotatorOptions* options) {
  auto out = std::make_unique<SlotModule>();
  out->tensor_index_map_ = tensor_index_map;
  out->mentioned_slot_threshold_ =
      options->mentioned_slot_threshold();
  TFLITE_RETURN_IF_ERROR(out->Init(interpreter, options));
  return out;
}

absl::Status SlotModule::Postprocess(Artifacts* artifacts,
                                     CluResponse* response) const {
  TFLITE_ASSIGN_OR_RETURN(
      const auto t_output,
      NamesAndConfidencesFromOutput(tensor_index_map_->slot_names_idx,
                                    tensor_index_map_->slot_scores_idx));
  const auto& [tags, confidences] = t_output;
  TFLITE_RETURN_IF_ERROR(SlotModulePopulateResponse(
      tags, confidences, artifacts->token_alignments, artifacts->token_turn_ids,
      artifacts->first_subword_indicators, mentioned_slot_threshold_,
      artifacts->reverse_utterance_list_to_encode, response));
  return absl::OkStatus();
}

}  // namespace tflite::task::text::clu
