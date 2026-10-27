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

#include "sentencepiece_trainer.h"

#include <string>
#include <vector>

#include "builder.h"
#include "common.h"
#include "normalizer.h"
#include "sentencepiece.pb.h"
#include "sentencepiece_model.pb.h"
#include "spec_parser.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "trainer_factory.h"
#include "util.h"

namespace sentencepiece {
namespace {
static constexpr char kDefaultNormalizerName[] = "nmt_nfkc";
static constexpr char kSerializedNormalizerSpecKey[] =
    "_serialized_normalizer_spec";

static constexpr const char* kNormalizerKeys[] = {
    "normalization_rule_name",
    "normalization_rule_tsv",
    "add_dummy_prefix",
    "escape_whitespaces",
    "remove_extra_whitespaces",
    "name",
    "precompiled_charsmap",
};

bool ContainsNormalizerKeys(
    const std::unordered_map<std::string, std::string>& kwargs,
    std::string* conflicting_key) {
  for (const char* key : kNormalizerKeys) {
    if (kwargs.find(key) != kwargs.end()) {
      if (conflicting_key) {
        *conflicting_key = key;
      }
      return true;
    }
  }
  return false;
}
}  // namespace

// static
absl::Status SentencePieceTrainer::Train(const TrainerSpec& trainer_spec,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  NormalizerSpec normalizer_spec;
  return Train(trainer_spec, normalizer_spec, sentence_iterator,
               serialized_model_proto);
}

absl::Status SentencePieceTrainer::Train(const TrainerSpec& trainer_spec,
                                         const NormalizerSpec& normalizer_spec,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  NormalizerSpec denormalizer_spec;
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

// static
absl::Status SentencePieceTrainer::Train(
    const TrainerSpec& trainer_spec, const NormalizerSpec& normalizer_spec,
    const NormalizerSpec& denormalizer_spec,
    SentenceIterator* sentence_iterator, std::string* serialized_model_proto) {
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

  ABSL_LOG(INFO) << "Starts training with : \n" << info;

  if (serialized_model_proto) {
    ModelProto model_proto;
    RETURN_IF_ERROR(trainer->Train(sentence_iterator, &model_proto));
    *serialized_model_proto = model_proto.SerializeAsString();
  } else {
    RETURN_IF_ERROR(trainer->Train(sentence_iterator, nullptr));
  }

  return absl::OkStatus();
}

// static
NormalizerSpec SentencePieceTrainer::GetNormalizerSpec(absl::string_view name) {
  NormalizerSpec spec;
  spec.set_name(name.data(), name.size());
  ABSL_CHECK_OK(normalizer::Builder::GetPrecompiledCharsMap(
      spec.name(), spec.mutable_precompiled_charsmap()));
  return spec;
}

// static
absl::Status SentencePieceTrainer::MergeSpecsFromArgs(
    absl::string_view args, TrainerSpec* trainer_spec,
    NormalizerSpec* normalizer_spec, NormalizerSpec* denormalizer_spec) {
  RET_CHECK(trainer_spec) << "`trainer_spec` must not be null.";
  RET_CHECK(normalizer_spec) << "`normalizer_spec` must not be null.";
  RET_CHECK(denormalizer_spec) << "`denormalizer_spec` must not be null.";

  if (args.empty()) return absl::OkStatus();

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
absl::Status SentencePieceTrainer::MergeSpecsFromArgs(
    const std::unordered_map<std::string, std::string>& kwargs,
    TrainerSpec* trainer_spec, NormalizerSpec* normalizer_spec,
    NormalizerSpec* denormalizer_spec) {
  RET_CHECK(trainer_spec) << "`trainer_spec` must not be null.";
  RET_CHECK(normalizer_spec) << "`normalizer_spec` must not be null.";
  RET_CHECK(denormalizer_spec) << "`denormalizer_spec` must not be null.";

  const auto it_spec = kwargs.find(kSerializedNormalizerSpecKey);
  const bool has_serialized_normalizer = (it_spec != kwargs.end());

  if (has_serialized_normalizer) {
    std::string conflicting_key;
    if (ContainsNormalizerKeys(kwargs, &conflicting_key)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Cannot specify both ", kSerializedNormalizerSpecKey,
          " and normalizer-specific argument: ", conflicting_key));
    }
  }

  // 1. Process serialized normalizer FIRST to load the base specs.
  // This is used by the Python module to support the 'normalizer' parameter.
  if (it_spec != kwargs.end()) {
    sentencepiece::NormalizerSpec spec;
    RET_CHECK(spec.ParseFromString(it_spec->second))
        << "Failed to parse " << kSerializedNormalizerSpecKey << ".";
    *normalizer_spec = spec;
  }

  for (const auto& [key, value] : kwargs) {
    if (key == kSerializedNormalizerSpecKey) {
      continue;
    }
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
      RET_CHECK(absl::SimpleAtoi(value, &v));
      SetMinLogLevel(v);
      continue;
    }

    const auto status_train = SetProtoField(key, value, trainer_spec);
    if (status_train.ok()) continue;
    if (!absl::IsNotFound(status_train)) return status_train;

    const auto status_norm = SetProtoField(key, value, normalizer_spec);
    if (status_norm.ok()) continue;
    if (!absl::IsNotFound(status_norm)) return status_norm;

    // Not found both in trainer_spec and normalizer_spec.
    if (absl::IsNotFound(status_train) && absl::IsNotFound(status_norm)) {
      return status_train;
    }
  }

  return absl::OkStatus();
}

// static
absl::Status SentencePieceTrainer::Train(absl::string_view args,
                                         SentenceIterator* sentence_iterator,
                                         std::string* serialized_model_proto) {
  ABSL_LOG(INFO) << "Running command: " << args.data();
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  RETURN_IF_ERROR(MergeSpecsFromArgs(args, &trainer_spec, &normalizer_spec,
                                     &denormalizer_spec));
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

// static
absl::Status SentencePieceTrainer::Train(
    const std::unordered_map<std::string, std::string>& kwargs,
    SentenceIterator* sentence_iterator, std::string* serialized_model_proto) {
  TrainerSpec trainer_spec;
  NormalizerSpec normalizer_spec;
  NormalizerSpec denormalizer_spec;
  RETURN_IF_ERROR(MergeSpecsFromArgs(kwargs, &trainer_spec, &normalizer_spec,
                                     &denormalizer_spec));
  return Train(trainer_spec, normalizer_spec, denormalizer_spec,
               sentence_iterator, serialized_model_proto);
}

namespace {
class VectorSentenceIterator : public SentenceIterator {
 public:
  explicit VectorSentenceIterator(const std::vector<std::string>& values)
      : iter_(values.begin()), end_(values.end()) {}
  virtual ~VectorSentenceIterator() {}
  virtual bool done() const { return iter_ == end_; }
  void Next() override { ++iter_; }
  const std::string& value() const override { return *iter_; }
  absl::Status status() const override { return absl::OkStatus(); }

 private:
  std::vector<std::string>::const_iterator iter_;
  std::vector<std::string>::const_iterator end_;
};
}  // namespace

// static
absl::Status SentencePieceTrainer::Train(
    absl::string_view args, const std::vector<std::string>& sentences,
    std::string* serialized_model_proto) {
  VectorSentenceIterator iter(sentences);
  return Train(args, &iter, serialized_model_proto);
}

// static
absl::Status SentencePieceTrainer::Train(
    const std::unordered_map<std::string, std::string>& kwargs,
    const std::vector<std::string>& sentences,
    std::string* serialized_model_proto) {
  VectorSentenceIterator iter(sentences);
  return Train(kwargs, &iter, serialized_model_proto);
}

// static
absl::Status SentencePieceTrainer::PopulateNormalizerSpec(
    NormalizerSpec* normalizer_spec, bool is_denormalizer) {
  RET_CHECK(normalizer_spec);

  if (!normalizer_spec->normalization_rule_tsv().empty()) {
    RET_CHECK(normalizer_spec->precompiled_charsmap().empty())
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

  return absl::OkStatus();
}

// static
absl::Status SentencePieceTrainer::PopulateModelTypeFromString(
    absl::string_view type, TrainerSpec* spec) {
  static const std::unordered_map<std::string, TrainerSpec::ModelType>
      kModelTypeMap = {{"unigram", TrainerSpec::UNIGRAM},
                       {"bpe", TrainerSpec::BPE},
                       {"word", TrainerSpec::WORD},
                       {"char", TrainerSpec::CHAR}};
  const auto it = kModelTypeMap.find(absl::AsciiStrToLower(type));
  if (it != kModelTypeMap.end()) {
    spec->set_model_type(it->second);
    return absl::OkStatus();
  }

  return absl::StatusBuilder(absl::StatusCode::kInternal)
         << "\"" << type << "\" is not found in TrainerSpec";
}

namespace {
const pretokenizer::PretokenizerForTrainingInterface* g_pretokenizer = nullptr;
}  // namespace

// static
absl::Status SentencePieceTrainer::SetPretokenizerForTraining(
    const pretokenizer::PretokenizerForTrainingInterface* pretokenizer) {
  g_pretokenizer = pretokenizer;
  return absl::OkStatus();
}

// static
const pretokenizer::PretokenizerForTrainingInterface*
SentencePieceTrainer::GetPretokenizerForTraining() {
  return g_pretokenizer;
}

SentencePieceNormalizer::SentencePieceNormalizer()
    : normalizer_spec_(std::make_unique<NormalizerSpec>()) {}
SentencePieceNormalizer::~SentencePieceNormalizer() {}

absl::Status SentencePieceNormalizer::Load(
    std::unique_ptr<ModelProto> model_proto) {
  RET_CHECK(model_proto) << "model_proto is null";
  return Load(std::make_unique<NormalizerSpec>(model_proto->normalizer_spec()));
}

absl::Status SentencePieceNormalizer::Load(
    std::unique_ptr<NormalizerSpec> normalizer_spec) {
  RET_CHECK(normalizer_spec) << "normalizer_spec is null";
  normalizer_spec_ = std::move(normalizer_spec);
  normalizer_ = std::make_unique<normalizer::Normalizer>(*normalizer_spec_);
  RET_CHECK(normalizer_);
  return normalizer_->status();
}

absl::Status SentencePieceNormalizer::Load(absl::string_view filename) {
  auto model_proto = std::make_unique<ModelProto>();
  RETURN_IF_ERROR(io::LoadModelProto(filename, model_proto.get()));
  return Load(std::move(model_proto));
}

absl::Status SentencePieceNormalizer::LoadFromSerializedProto(
    absl::string_view serialized) {
  auto model_proto = std::make_unique<ModelProto>();
  RET_CHECK(model_proto->ParseFromArray(serialized.data(), serialized.size()));
  return Load(std::move(model_proto));
}

absl::Status SentencePieceNormalizer::LoadFromSerializedNormalizerSpec(
    absl::string_view serialized) {
  auto spec = std::make_unique<NormalizerSpec>();
  RET_CHECK(spec->ParseFromArray(serialized.data(), serialized.size()));
  return Load(std::move(spec));
}

absl::Status SentencePieceNormalizer::LoadFromRuleTSV(
    absl::string_view filename) {
  auto spec = std::make_unique<NormalizerSpec>();
  spec->set_normalization_rule_tsv(filename.data(), filename.size());
  RETURN_IF_ERROR(SentencePieceTrainer::PopulateNormalizerSpec(spec.get()));
  return Load(std::move(spec));
}

absl::Status SentencePieceNormalizer::LoadFromRuleName(absl::string_view name) {
  auto spec = std::make_unique<NormalizerSpec>();
  spec->set_name(name.data(), name.size());
  RETURN_IF_ERROR(SentencePieceTrainer::PopulateNormalizerSpec(spec.get()));
  return Load(std::move(spec));
}

absl::Status SentencePieceNormalizer::LoadFromMap(
    absl::Span<const std::pair<std::string, std::string>> norm_map) {
  normalizer::Builder::CharsMap chars_map;
  for (const auto &[src, trg] : norm_map) {
    if (!string_util::IsStructurallyValid(src)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid UTF-8 sequence in source: ", src));
    }
    if (!string_util::IsStructurallyValid(trg)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid UTF-8 sequence in target: ", trg));
    }
    auto src_uni = string_util::UTF8ToUnicodeText(src);
    auto trg_uni = string_util::UTF8ToUnicodeText(trg);
    if (!chars_map.emplace(std::move(src_uni), std::move(trg_uni)).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("Duplicate source string in map: ", src));
    }
  }
  std::string blob;
  RETURN_IF_ERROR(normalizer::Builder::CompileCharsMap(chars_map, &blob));
  auto spec = std::make_unique<NormalizerSpec>();
  spec->set_precompiled_charsmap(blob);
  return Load(std::move(spec));
}

absl::Status SentencePieceNormalizer::Decompile(
    std::vector<std::pair<std::string, std::string>> *norm_map) const {
  RET_CHECK(normalizer_spec_);
  const auto &blob = normalizer_spec_->precompiled_charsmap();
  RET_CHECK(!blob.empty()) << "No precompiled charsmap found in model.";
  normalizer::Builder::CharsMap chars_map;
  RETURN_IF_ERROR(normalizer::Builder::DecompileCharsMap(blob, &chars_map));
  norm_map->clear();
  for (const auto &[key, val] : chars_map) {
    std::string src = string_util::UnicodeTextToUTF8(key);
    std::string trg = string_util::UnicodeTextToUTF8(val);
    norm_map->emplace_back(std::move(src), std::move(trg));
  }
  return absl::OkStatus();
}

absl::Status SentencePieceNormalizer::Normalize(absl::string_view input,
                                                std::string* normalized) const {
  RET_CHECK(normalizer_);
  std::vector<size_t> norm_to_orig;
  return normalizer_->Normalize(input, normalized, &norm_to_orig);
}

absl::Status SentencePieceNormalizer::Normalize(
    absl::string_view input, std::string* normalized,
    std::vector<size_t>* norm_to_orig) const {
  RET_CHECK(normalizer_);
  return normalizer_->Normalize(input, normalized, norm_to_orig);
}

std::string SentencePieceNormalizer::Normalize(absl::string_view input) const {
  std::string normalized;
  Normalize(input, &normalized).IgnoreError();
  return normalized;
}

NormalizerSpec* SentencePieceNormalizer::mutable_normalizer_spec() {
  return normalizer_spec_.get();
}

std::string SentencePieceNormalizer::serialized_model_proto() const {
  if (!normalizer_spec_) return "";
  ModelProto model_proto;
  *model_proto.mutable_normalizer_spec() = *normalizer_spec_;
  return model_proto.SerializeAsString();
}

std::string SentencePieceNormalizer::serialized_normalizer_spec() const {
  return normalizer_spec_ ? normalizer_spec_->SerializeAsString() : "";
}

void ConvertToUnicodeAlignment(absl::string_view orig, absl::string_view norm,
                               std::vector<size_t>* norm_to_orig) {
  auto utf8_to_unicode_offsets = [](absl::string_view str) {
    std::vector<int> utf8_to_unicode(str.size() + 1, 0);
    size_t prev = 0;
    int ulen = 0;
    while (!str.empty()) {
      const size_t mblen =
          std::min(str.size(), static_cast<size_t>(std::max<int>(
                                   1, string_util::OneCharLen(str.data()))));
      for (size_t i = prev; i < prev + mblen; ++i) {
        utf8_to_unicode[i] = ulen;
      }
      ++ulen;
      prev += mblen;
      str.remove_prefix(mblen);
    }
    utf8_to_unicode[prev] = ulen;
    return utf8_to_unicode;
  };

  const auto orig_offsets = utf8_to_unicode_offsets(orig);
  const auto norm_offsets = utf8_to_unicode_offsets(norm);
  if (orig_offsets.empty() || norm_offsets.empty()) return;

  std::vector<size_t> result(norm_offsets.back() + 1, 0);
  for (size_t i = 0; i < norm_to_orig->size(); ++i) {
    result[norm_offsets[i]] = orig_offsets[(*norm_to_orig)[i]];
  }
  *norm_to_orig = std::move(result);
}

}  // namespace sentencepiece
