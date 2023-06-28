// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#ifndef SENTENCEPIECE_TRAINER_H_
#define SENTENCEPIECE_TRAINER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "src/sentencepiece_processor.h"

namespace sentencepiece {

class TrainerSpec;
class NormalizerSpec;

namespace pretokenizer {
class PretokenizerForTrainingInterface;
}  // namespace pretokenizer

class SentencePieceTrainer {
 public:
  // Trains SentencePiece model with `trainer_spec`.
  // Default `normalizer_spec` is used.
  static ::util::Status Train(const TrainerSpec &trainer_spec);

  // Trains SentencePiece model with `trainer_spec` and
  // `normalizer_spec`.
  static ::util::Status Train(const TrainerSpec &trainer_spec,
                              const NormalizerSpec &normalizer_spec);

  // Trains SentencePiece model with command-line string in `args`,
  // e.g.,
  // '--input=data --model_prefix=m --vocab_size=8192 model_type=unigram'
  static ::util::Status Train(absl::string_view args);

  // Handy function to make a normalizer spec from the pre-compiled
  // normalization name. Do not use this method in production as it crashes
  // when `name` is invalid. Useful for unittesting.
  static NormalizerSpec GetNormalizerSpec(absl::string_view name);

  // Populates necessary fields (precompiled_charmap) from
  // `NormalizerSpec::name` or `NormalizerSpec::normalization_rule_tsv`.
  static ::util::Status PopulateNormalizerSpec(NormalizerSpec *normalizer_spec);

  // Overrides `trainer_spec` and `normalizer_spec` with the
  // command-line string in `args`.
  static ::util::Status MergeSpecsFromArgs(absl::string_view args,
                                           TrainerSpec *trainer_spec,
                                           NormalizerSpec *normalizer_spec);

  // Injects global pre-tokenizer that are applied in training time.
  // Pretokenizer is only used for extracting pieces.
  // TODO(taku): It would be better to inject per `trainer_spec`.
  static ::util::Status SetPretokenizerForTraining(
      const pretokenizer::PretokenizerForTrainingInterface *pretokenizer);

  // Returns the current pretokenizer. if no pretokenizer is defined, returns
  // nullptr.
  static const pretokenizer::PretokenizerForTrainingInterface *
  GetPretokenizerForTraining();

  // Helper function to set `field_name=value` in `message`.
  // When `field_name` is repeated, multiple values can be passed
  // with comma-separated values. `field_name` must not be a nested message.
  // The body of these functions are automatically generated with
  // data/gen_spec_parser.pl
  static ::util::Status SetProtoField(const std::string &name,
                                      const std::string &value,
                                      TrainerSpec *message);

  static ::util::Status SetProtoField(const std::string &name,
                                      const std::string &value,
                                      NormalizerSpec *message);

  // Populates model type from string representation, e.g., "bpe".
  // Supported model: "unigram", "bpe", "word", "char".
  static ::util::Status PopulateModelTypeFromString(absl::string_view type,
                                                    TrainerSpec *trainer_spec);

  SentencePieceTrainer() = delete;
  ~SentencePieceTrainer() = delete;
};
}  // namespace sentencepiece
#endif  // SENTENCEPIECE_TRAINER_H_
