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

#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "builder.h"
#include "common.h"
#include "normalizer.h"
#include "sentencepiece.pb.h"
#include "sentencepiece_model.pb.h"
#include "sentencepiece_trainer.h"
#include "spec_parser.h"
#include "trainer_factory.h"
#include "util.h"

namespace sentencepiece {
namespace {
static constexpr char kDefaultNormalizerName[] = "nmt_nfkc";
}  // namespace

// static
util::Status SentencePieceTrainer::Train(const TrainerSpec& trainer_spec,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  NormalizerSpec normalizer_spec;
  return Train(trainer_spec, normalizer_spec, sentence_iterator,
               serialized_model_proto);
}

util::Status SentencePieceTrainer::Train(const TrainerSpec& trainer_spec,
                                         const NormalizerSpec& normalizer_spec,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  NormalizerSpec denormalizer_spec;
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

// static
util::Status SentencePieceTrainer::Train(
    const TrainerSpec& trainer_spec,
    const NormalizerSpec& normalizer_spec,
    const NormalizerSpec& denormalizer_spec,
    SentenceIterator* sentence_iterator,
    std::string* serialized_model_proto) {
  auto copied_normalizer_spec = normalizer_spec;
  RETURN_IF_ERROR(PopulateNormalizerSpec(&copied_normalizer_spec, false));
  auto copied_denormalizer_spec = denormalizer_spec;
  RETURN_IF_ERROR(PopulateNormalizerSpec(&copied_denormalizer_spec, true));
  auto trainer = TrainerFactory::Create(trainer_spec, copied_normalizer_spec,
                                        copied_denormalizer_spec);
  std::string info =
      absl::StrCat(PrintProto(trainer_spec, "trainer_spec"),
                   PrintProto(copied_normalizer_spec, "normalizer_spec"));
  if (!copied_denormalizer_spec.precompiled_charsmap().empty()) {
    info += PrintProto(copied_denormalizer_spec, "denormalizer_spec");
  } else {
    info += "denormalizer_spec {}";
  }

  LOG(INFO) << "Starts training with : \n" << info;

  if (serialized_model_proto) {
    ModelProto model_proto;
    RETURN_IF_ERROR(trainer->Train(sentence_iterator, &model_proto));
    *serialized_model_proto = model_proto.SerializeAsString();
  } else {
    RETURN_IF_ERROR(trainer->Train(sentence_iterator, nullptr));
  }

  return util::OkStatus();
}

// static
NormalizerSpec SentencePieceTrainer::GetNormalizerSpec(absl::string_view name) {
  NormalizerSpec spec;
  spec.set_name(name.data(), name.size());
  CHECK_OK(normalizer::Builder::GetPrecompiledCharsMap(
      spec.name(), spec.mutable_precompiled_charsmap()));
  return spec;
}

// static
util::Status SentencePieceTrainer::MergeSpecsFromArgs(
    absl::string_view args,
    TrainerSpec* trainer_spec,
    NormalizerSpec* normalizer_spec,
    NormalizerSpec* denormalizer_spec) {
  CHECK_OR_RETURN(trainer_spec) << "`trainer_spec` must not be null.";
  CHECK_OR_RETURN(normalizer_spec) << "`normalizer_spec` must not be null.";
  CHECK_OR_RETURN(denormalizer_spec) << "`denormalizer_spec` must not be null.";

  if (args.empty()) {
    return util::OkStatus();
  }

  std::unordered_map<std::string, std::string> kwargs;
  for (auto arg : absl::StrSplit(args, " ")) {
    absl::ConsumePrefix(&arg, "--");
    std::string key, value;
    const auto pos = arg.find('=');
    if (pos == absl::string_view::npos) {
      key = std::string(arg);
    } else {
      key = std::string(arg.substr(0, pos));
      value = std::string(arg.substr(pos + 1));
    }
    kwargs.emplace(key, value);
  }

  return MergeSpecsFromArgs(kwargs, trainer_spec, normalizer_spec,
                            denormalizer_spec);
}

// static
util::Status SentencePieceTrainer::MergeSpecsFromArgs(
    const std::unordered_map<std::string, std::string>& kwargs,
    TrainerSpec* trainer_spec,
    NormalizerSpec* normalizer_spec,
    NormalizerSpec* denormalizer_spec) {
  CHECK_OR_RETURN(trainer_spec) << "`trainer_spec` must not be null.";
  CHECK_OR_RETURN(normalizer_spec) << "`normalizer_spec` must not be null.";
  CHECK_OR_RETURN(denormalizer_spec) << "`denormalizer_spec` must not be null.";

  for (const auto& it : kwargs) {
    const auto& key = it.first;
    const auto& value = it.second;
    // Exceptions.
    if (key == "normalization_rule_name") {
      normalizer_spec->set_name(value);
      continue;
    } else if (key == "denormalization_rule_tsv") {
      denormalizer_spec->set_normalization_rule_tsv(value);
      denormalizer_spec->set_add_dummy_prefix(false);
      denormalizer_spec->set_remove_extra_whitespaces(false);
      denormalizer_spec->set_escape_whitespaces(false);
      continue;
    } else if (key == "minloglevel") {
      int v = 0;
      CHECK_OR_RETURN(absl::SimpleAtoi(value, &v));
      logging::SetMinLogLevel(v);
      continue;
    }

    const auto status_train = SetProtoField(key, value, trainer_spec);
    if (status_train.ok()) continue;
    if (!util::IsNotFound(status_train)) {
      return status_train;
    }

    const auto status_norm = SetProtoField(key, value, normalizer_spec);
    if (status_norm.ok()) continue;
    if (!util::IsNotFound(status_norm)) {
      return status_norm;
    }

    // Not found both in trainer_spec and normalizer_spec.
    if (util::IsNotFound(status_train) && util::IsNotFound(status_norm)) {
      return status_train;
    }
  }

  return util::OkStatus();
}

// static
util::Status SentencePieceTrainer::Train(absl::string_view args,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  LOG(INFO) << "Running command: " << args.data();
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  RETURN_IF_ERROR(MergeSpecsFromArgs(args, &trainer_spec, &normalizer_spec,
                                     &denormalizer_spec));
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

// static
util::Status SentencePieceTrainer::Train(
    const std::unordered_map<std::string, std::string>& kwargs,
    SentenceIterator* sentence_iterator,
    std::string* serialized_model_proto) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  RETURN_IF_ERROR(MergeSpecsFromArgs(kwargs, &trainer_spec, &normalizer_spec,
                                     &denormalizer_spec));
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

// static
util::Status SentencePieceTrainer::PopulateNormalizerSpec(
    NormalizerSpec* normalizer_spec,
    bool is_denormalizer) {
  CHECK_OR_RETURN(normalizer_spec);

  if (!normalizer_spec->normalization_rule_tsv().empty()) {
    CHECK_OR_RETURN(normalizer_spec->precompiled_charsmap().empty())
        << "precompiled_charsmap is already defined.";
    normalizer::Builder::CharsMap chars_map;
    RETURN_IF_ERROR(normalizer::Builder::LoadCharsMap(
        normalizer_spec->normalization_rule_tsv(), &chars_map));
    RETURN_IF_ERROR(normalizer::Builder::CompileCharsMap(
        chars_map, normalizer_spec->mutable_precompiled_charsmap()));
    normalizer_spec->set_name("user_defined");
  } else if (!is_denormalizer) {
    if (normalizer_spec->name().empty()) {
      normalizer_spec->set_name(kDefaultNormalizerName);
    }
    if (normalizer_spec->precompiled_charsmap().empty()) {
      RETURN_IF_ERROR(normalizer::Builder::GetPrecompiledCharsMap(
          normalizer_spec->name(),
          normalizer_spec->mutable_precompiled_charsmap()));
    }
  }

  return util::OkStatus();
}

// static
util::Status SentencePieceTrainer::PopulateModelTypeFromString(
    absl::string_view type,
    TrainerSpec* spec) {
  static const std::unordered_map<std::string, TrainerSpec::ModelType>
      kModelTypeMap = {{"unigram", TrainerSpec::UNIGRAM},
                       {"bpe", TrainerSpec::BPE},
                       {"word", TrainerSpec::WORD},
                       {"char", TrainerSpec::CHAR}};
  const auto it = kModelTypeMap.find(absl::AsciiStrToLower(type));
  if (it != kModelTypeMap.end()) {
    spec->set_model_type(it->second);
    return util::OkStatus();
  }

  return util::StatusBuilder(util::StatusCode::kInternal, GTL_LOC)
         << "\"" << type << "\" is not found in TrainerSpec";
}

namespace {
const pretokenizer::PretokenizerForTrainingInterface *g_pretokenizer = nullptr;
}  // namespace

// static
util::Status SentencePieceTrainer::SetPretokenizerForTraining(
    const pretokenizer::PretokenizerForTrainingInterface* pretokenizer) {
  g_pretokenizer = pretokenizer;
  return util::OkStatus();
}

// static
const pretokenizer::PretokenizerForTrainingInterface *
SentencePieceTrainer::GetPretokenizerForTraining() {
  return g_pretokenizer;
}

}  // namespace sentencepiece
