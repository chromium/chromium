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

#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_utils.h"

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/regex_tokenizer.h"
#include "tensorflow_lite_support/metadata/metadata_schema_generated.h"

namespace tflite {
namespace support {
namespace text {
namespace tokenizer {

using ::tflite::ProcessUnit;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

namespace {

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
  TFLITE_ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                   metadata_extractor->GetAssociatedFile(
                       associated_files->Get(0)->name()->str()));
  return vocab_buffer;
}
}  // namespace

StatusOr<std::unique_ptr<Tokenizer>> CreateTokenizerFromProcessUnit(
    const tflite::ProcessUnit* tokenizer_process_unit,
    const tflite::metadata::ModelMetadataExtractor* metadata_extractor) {
  if (metadata_extractor == nullptr || tokenizer_process_unit == nullptr) {
    return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "No metadata or input process unit found.",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
  switch (tokenizer_process_unit->options_type()) {
    case ProcessUnitOptions_BertTokenizerOptions: {
      const tflite::BertTokenizerOptions* options =
          tokenizer_process_unit->options_as<tflite::BertTokenizerOptions>();
      TFLITE_ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                       CheckAndLoadFirstAssociatedFile(options->vocab_file(),
                                                       metadata_extractor));
      return absl::make_unique<BertTokenizer>(vocab_buffer.data(),
                                              vocab_buffer.size());
    }
    case ProcessUnitOptions_SentencePieceTokenizerOptions: {
      return CreateStatusWithPayload(
        absl::StatusCode::kInvalidArgument,
        "Chromium does not support sentencepiece tokenization",
        TfLiteSupportStatus::kMetadataInvalidTokenizerError);
    }
    case ProcessUnitOptions_RegexTokenizerOptions: {
      const tflite::RegexTokenizerOptions* options =
          tokenizer_process_unit->options_as<RegexTokenizerOptions>();
      TFLITE_ASSIGN_OR_RETURN(absl::string_view vocab_buffer,
                       CheckAndLoadFirstAssociatedFile(options->vocab_file(),
                                                       metadata_extractor));
      if (options->delim_regex_pattern() == nullptr) {
        return CreateStatusWithPayload(
            absl::StatusCode::kInvalidArgument,
            "Invalid delim_regex_pattern from input process unit.",
            TfLiteSupportStatus::kMetadataInvalidTokenizerError);
      }

      std::unique_ptr<RegexTokenizer> regex_tokenizer =
          absl::make_unique<RegexTokenizer>(
              options->delim_regex_pattern()->str(), vocab_buffer.data(),
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

      return std::move(regex_tokenizer);
    }
    default:
      return CreateStatusWithPayload(
          absl::StatusCode::kNotFound,
          absl::StrCat("Incorrect options_type:",
                       tokenizer_process_unit->options_type()),
          TfLiteSupportStatus::kMetadataInvalidTokenizerError);
  }
}

}  // namespace tokenizer
}  // namespace text
}  // namespace support
}  // namespace tflite
