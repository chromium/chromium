// Copyright 2016 Google Inc.
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

#include "src/sentencepiece_processor.h"

#include <map>
#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "src/common.h"
#include "src/filesystem.h"
#include "src/model_factory.h"
#include "src/model_interface.h"
#include "src/normalizer.h"
#include "src/sentencepiece.pb.h"
#include "src/unigram_model.h"
#include "src/util.h"

namespace sentencepiece {
namespace {

// Replaces white space with U+2581 (LOWER ONE EIGHT BLOCK).
const char kSpaceSymbol[] = "\xe2\x96\x81";

// Encodes <unk> into U+2047 (DOUBLE QUESTION MARK),
// since this character can be useful both for user and
// developer. We can easily figure out that <unk> is emitted.
const char kDefaultUnknownSymbol[] = " \xE2\x81\x87 ";
}  // namespace

SentencePieceProcessor::SentencePieceProcessor() {}
SentencePieceProcessor::~SentencePieceProcessor() {}

::util::Status SentencePieceProcessor::Load(absl::string_view filename) {
  auto model_proto = absl::make_unique<ModelProto>();
  RETURN_IF_ERROR(io::LoadModelProto(filename, model_proto.get()));
  return Load(std::move(model_proto));
}

void SentencePieceProcessor::LoadOrDie(absl::string_view filename) {
  CHECK_OK(Load(filename));
}

::util::Status SentencePieceProcessor::Load(const ModelProto &model_proto) {
  auto model_proto_copy = absl::make_unique<ModelProto>();
  *model_proto_copy = model_proto;
  return Load(std::move(model_proto_copy));
}

::util::Status SentencePieceProcessor::LoadFromSerializedProto(
    absl::string_view serialized) {
  auto model_proto = absl::make_unique<ModelProto>();
  CHECK_OR_RETURN(
      model_proto->ParseFromArray(serialized.data(), serialized.size()));
  return Load(std::move(model_proto));
}

::util::Status SentencePieceProcessor::Load(
    std::unique_ptr<ModelProto> model_proto) {
  model_proto_ = std::move(model_proto);
  model_ = ModelFactory::Create(*model_proto_);
  normalizer_ = absl::make_unique<normalizer::Normalizer>(
      model_proto_->normalizer_spec(), model_proto_->trainer_spec());

  // Escapes user-defined-symbols in normalizer.
  normalizer_->SetPrefixMatcher(model_->prefix_matcher());

  RETURN_IF_ERROR(status());

  // Running self-testing.
  std::vector<std::string> errors, sps;
  for (const auto &s : model_proto_->self_test_data().samples()) {
    RETURN_IF_ERROR(Encode(s.input(), &sps));
    const std::string result = absl::StrJoin(sps, " ");
    if (s.expected() != result) {
      errors.emplace_back(
          absl::StrCat(s.input(), "\t", s.expected(), "\t", result));
    }
  }

  if (!errors.empty()) {
    LOG(INFO) << errors.size() << "/"
              << model_proto_->self_test_data().samples_size()
              << " samples did not pass the test.";
    for (const auto &e : errors) {
      LOG(INFO) << e;
    }
    return ::util::InternalError("Self-test failures. See LOG(INFO).");
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::SetEncodeExtraOptions(
    absl::string_view extra_options) {
  return ParseExtraOptions(extra_options, &encode_extra_options_);
}

::util::Status SentencePieceProcessor::SetDecodeExtraOptions(
    absl::string_view extra_options) {
  return ParseExtraOptions(extra_options, &decode_extra_options_);
}

::util::Status SentencePieceProcessor::status() const {
  CHECK_OR_RETURN(model_) << "Model is not initialized.";
  CHECK_OR_RETURN(normalizer_) << "Normalizer is not initialized.";
  RETURN_IF_ERROR(model_->status());
  RETURN_IF_ERROR(normalizer_->status());
  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::SetVocabulary(
    const std::vector<std::string> &valid_vocab) {
  RETURN_IF_ERROR(status());

  // TODO(taku): supports vocabulary constraint in BPE model.
  const auto type = model_proto_->trainer_spec().model_type();
  CHECK_OR_RETURN(type == TrainerSpec::UNIGRAM || type == TrainerSpec::BPE)
      << "Vocabulary constraint is only enabled in subword units.";

  const std::set<std::string> vocab(valid_vocab.begin(), valid_vocab.end());

  for (int i = 0; i < model_proto_->pieces_size(); ++i) {
    auto *piece = model_proto_->mutable_pieces(i);
    if (piece->type() == ModelProto::SentencePiece::CONTROL ||
        piece->type() == ModelProto::SentencePiece::UNKNOWN ||
        piece->type() == ModelProto::SentencePiece::USER_DEFINED) {
      continue;
    }
    if (vocab.find(piece->piece()) != vocab.end() ||
        string_util::OneCharLen(piece->piece().c_str()) ==
            piece->piece().size()) {
      piece->set_type(ModelProto::SentencePiece::NORMAL);
    } else {
      piece->set_type(ModelProto::SentencePiece::UNUSED);
    }
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::ResetVocabulary() {
  RETURN_IF_ERROR(status());
  for (auto &piece : *(model_proto_->mutable_pieces())) {
    if (piece.type() == ModelProto::SentencePiece::UNUSED)
      piece.set_type(ModelProto::SentencePiece::NORMAL);
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::LoadVocabulary(
    absl::string_view filename, int threshold) {
  auto input = filesystem::NewReadableFile(filename);
  RETURN_IF_ERROR(input->status());

  std::string line;
  std::vector<std::string> vocab;

  while (input->ReadLine(&line)) {
    const std::vector<std::string> v = absl::StrSplit(line, "\t");
    CHECK_GE_OR_RETURN(v.size(), 1);
    CHECK_OR_RETURN(!v[0].empty());
    int32 freq = 1;
    if (v.size() >= 2) {
      CHECK_OR_RETURN(absl::SimpleAtoi(v[1], &freq))
          << "Could not parse the frequency";
    }
    if (freq >= threshold) {
      vocab.emplace_back(v[0]);
    }
  }

  return SetVocabulary(vocab);
}

#define CHECK_OR_RETURN_STATUS_STL(container)               \
  RETURN_IF_ERROR(status());                                \
  CHECK_OR_RETURN(container) << "output container is null"; \
  container->clear();

#define CHECK_OR_RETURN_STATUS_PROTO(proto)         \
  RETURN_IF_ERROR(status());                        \
  CHECK_OR_RETURN(proto) << "output proto is null"; \
  proto->Clear();

//////////////////////////////////////////////////////////////
// Simple API.
::util::Status SentencePieceProcessor::Encode(
    absl::string_view input, std::vector<std::string> *pieces) const {
  CHECK_OR_RETURN_STATUS_STL(pieces);

  SentencePieceText spt;
  RETURN_IF_ERROR(Encode(input, &spt));
  for (const auto &sp : spt.pieces()) {
    pieces->emplace_back(sp.piece());
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::Encode(absl::string_view input,
                                              std::vector<int> *ids) const {
  CHECK_OR_RETURN_STATUS_STL(ids);

  SentencePieceText spt;
  RETURN_IF_ERROR(Encode(input, &spt));
  for (const auto &sp : spt.pieces()) {
    ids->emplace_back(sp.id());
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::Decode(
    const std::vector<std::string> &pieces, std::string *detokenized) const {
  CHECK_OR_RETURN_STATUS_STL(detokenized);

  SentencePieceText spt;
  RETURN_IF_ERROR(Decode(pieces, &spt));
  *detokenized = std::move(spt.text());

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::Decode(const std::vector<int> &ids,
                                              std::string *detokenized) const {
  CHECK_OR_RETURN_STATUS_STL(detokenized);

  SentencePieceText spt;
  RETURN_IF_ERROR(Decode(ids, &spt));
  *detokenized = std::move(spt.text());

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    std::vector<std::vector<std::string>> *pieces) const {
  CHECK_OR_RETURN_STATUS_STL(pieces);

  NBestSentencePieceText spt;
  RETURN_IF_ERROR(NBestEncode(input, nbest_size, &spt));
  for (const auto &nbest : spt.nbests()) {
    std::vector<std::string> result;
    for (const auto &sp : nbest.pieces()) {
      result.emplace_back(sp.piece());
    }
    pieces->emplace_back(result);
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    std::vector<std::vector<int>> *ids) const {
  CHECK_OR_RETURN_STATUS_STL(ids);

  NBestSentencePieceText spt;
  RETURN_IF_ERROR(NBestEncode(input, nbest_size, &spt));
  for (const auto &nbest : spt.nbests()) {
    std::vector<int> result;
    for (const auto &sp : nbest.pieces()) {
      result.emplace_back(sp.id());
    }
    ids->emplace_back(result);
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::SampleEncode(
    absl::string_view input, int nbest_size, float alpha,
    std::vector<std::string> *pieces) const {
  CHECK_OR_RETURN_STATUS_STL(pieces);

  SentencePieceText spt;
  RETURN_IF_ERROR(SampleEncode(input, nbest_size, alpha, &spt));
  for (const auto &sp : spt.pieces()) {
    pieces->emplace_back(sp.piece());
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::SampleEncode(
    absl::string_view input, int nbest_size, float alpha,
    std::vector<int> *ids) const {
  CHECK_OR_RETURN_STATUS_STL(ids);

  SentencePieceText spt;
  RETURN_IF_ERROR(SampleEncode(input, nbest_size, alpha, &spt));
  for (const auto &sp : spt.pieces()) {
    ids->emplace_back(sp.id());
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::PopulateSentencePieceText(
    absl::string_view input, absl::string_view normalized,
    const std::vector<size_t> &norm_to_orig, const EncodeResult &result,
    SentencePieceText *spt) const {
  size_t consumed = 0;
  bool is_prev_unk = false;
  for (const auto &p : result) {
    const absl::string_view w = p.first;  // piece
    const int id = p.second;              // id

    CHECK_OR_RETURN(!w.empty()) << "Empty piece is not allowed.";

    const bool is_unk = IsUnknown(id);

    if (IsControl(id)) {
      // Control symbol has no corresponding source surface, so begin == end.
      auto *sp = spt->add_pieces();
      sp->set_piece(w.data(), w.size());
      sp->set_id(id);
      sp->set_begin(norm_to_orig[consumed]);
      sp->set_end(norm_to_orig[consumed]);
    } else {
      const size_t begin = consumed;
      const size_t end = consumed + w.size();
      CHECK_LT_OR_RETURN(begin, norm_to_orig.size());
      CHECK_LT_OR_RETURN(end, norm_to_orig.size());
      const size_t orig_begin = norm_to_orig[begin];
      const size_t orig_end = norm_to_orig[end];
      CHECK_LE_OR_RETURN(orig_begin, input.size());
      CHECK_LE_OR_RETURN(orig_end, input.size());
      CHECK_LE_OR_RETURN(orig_begin, orig_end);
      const auto surface =
          absl::ClippedSubstr(input.data(), orig_begin, orig_end - orig_begin);
      // Merges continuous run of unknown pieces so that decoder
      // can copy or generate unknown tokens easily.
      // Note that merged tokens are still unknown,
      // since known pieces never consist of unknown characters.
      if (is_prev_unk && is_unk) {
        auto *sp = spt->mutable_pieces(spt->pieces_size() - 1);
        sp->set_piece(sp->piece() + std::string(w));
        sp->set_surface(sp->surface() + std::string(surface));
        sp->set_end(orig_end);
      } else {
        auto *sp = spt->add_pieces();
        sp->set_piece(w.data(), w.size());
        sp->set_id(id);
        sp->set_surface(surface.data(), surface.size());
        sp->set_begin(orig_begin);
        sp->set_end(orig_end);
      }
      consumed += w.size();
    }
    is_prev_unk = is_unk;
  }

  CHECK_EQ_OR_RETURN(consumed, normalized.size())
      << "all normalized characters are not consumed.";

  RETURN_IF_ERROR(ApplyExtraOptions(encode_extra_options_, spt));

  spt->set_text(input.data(), input.size());

  return ::util::OkStatus();
}  // namespace sentencepiece

::util::Status SentencePieceProcessor::Encode(absl::string_view input,
                                              SentencePieceText *spt) const {
  CHECK_OR_RETURN_STATUS_PROTO(spt);

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  const auto result = model_->Encode(normalized);
  RETURN_IF_ERROR(
      PopulateSentencePieceText(input, normalized, norm_to_orig, result, spt));

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    NBestSentencePieceText *nbest_spt) const {
  CHECK_OR_RETURN_STATUS_PROTO(nbest_spt);

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  const auto nbests = model_->NBestEncode(normalized, nbest_size);
  CHECK_OR_RETURN(!nbests.empty()) << "NBestEncode returns empty result.";

  for (const auto &result : nbests) {
    auto *spt = nbest_spt->add_nbests();
    spt->set_score(result.second);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result.first, spt));
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::SampleEncode(
    absl::string_view input, int nbest_size, float alpha,
    SentencePieceText *spt) const {
  CHECK_OR_RETURN_STATUS_PROTO(spt);

  CHECK_LE_OR_RETURN(nbest_size, 512) << "nbest_size must be nbest_size <= 512";

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  if (nbest_size == 1 || nbest_size == 0) {
    const auto result = model_->Encode(normalized);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result, spt));
  } else if (nbest_size > 1) {
    const auto nbests = model_->NBestEncode(normalized, nbest_size);
    CHECK_OR_RETURN(!nbests.empty()) << "NBestEncode returns empty result.";

    std::vector<float> probs(nbests.size(), 0.0);
    for (size_t i = 0; i < nbests.size(); ++i) {
      probs[i] = std::exp(alpha * nbests[i].second);
    }

    auto *mt = random::GetRandomGenerator();
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              nbests[dist(*mt)].first, spt));

  } else if (nbest_size < 0) {
    const auto result = model_->SampleEncode(normalized, alpha);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result, spt));
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::Decode(
    const std::vector<std::string> &pieces, SentencePieceText *spt) const {
  CHECK_OR_RETURN_STATUS_PROTO(spt);

  const char *unk_surface = kDefaultUnknownSymbol;
  if (model_proto_ && model_proto_->trainer_spec().has_unk_surface())
    unk_surface = model_proto_->trainer_spec().unk_surface().c_str();

  auto DecodeSentencePiece = [&](absl::string_view piece, int id,
                                 bool is_bos_ws) -> std::string {
    if (IsControl(id)) {  // <s>, </s>
      return "";          // invisible symbol.
    } else if (IsUnknown(id)) {
      if (IdToPiece(id) == piece) {  // <unk>
        return unk_surface;
      } else {  // return piece when piece is not <unk>.
        return std::string(piece);
      }
    }

    if (is_bos_ws) {
      // Consume if the current position is bos and
      // piece starts with kSpaceSymbol.
      absl::ConsumePrefix(&piece, kSpaceSymbol);
    }

    return absl::StrReplaceAll(piece, {{kSpaceSymbol, " "}});
  };

  for (const std::string &w : pieces) {
    auto *sp = spt->add_pieces();
    sp->set_piece(w);
    sp->set_id(PieceToId(w));
  }

  RETURN_IF_ERROR(ApplyExtraOptions(decode_extra_options_, spt));

  std::string *text = spt->mutable_text();
  for (auto &sp : *(spt->mutable_pieces())) {
    sp.set_surface(DecodeSentencePiece(sp.piece(), sp.id(), text->empty()));
    sp.set_begin(text->size());
    sp.set_end(text->size() + sp.surface().size());
    *text += sp.surface();
  }

  return ::util::OkStatus();
}

::util::Status SentencePieceProcessor::Decode(const std::vector<int> &ids,
                                              SentencePieceText *spt) const {
  std::vector<std::string> pieces;
  pieces.reserve(ids.size());
  for (const int id : ids) {
    pieces.emplace_back(IdToPiece(id));
  }
  return Decode(pieces, spt);
}

std::string SentencePieceProcessor::EncodeAsSerializedProto(
    absl::string_view input) const {
  SentencePieceText spt;
  if (!Encode(input, &spt).ok()) return "";
  return spt.SerializeAsString();
}

std::string SentencePieceProcessor::SampleEncodeAsSerializedProto(
    absl::string_view input, int nbest_size, float alpha) const {
  SentencePieceText spt;
  if (!SampleEncode(input, nbest_size, alpha, &spt).ok()) return "";
  return spt.SerializeAsString();
}

std::string SentencePieceProcessor::NBestEncodeAsSerializedProto(
    absl::string_view input, int nbest_size) const {
  NBestSentencePieceText spt;
  if (!NBestEncode(input, nbest_size, &spt).ok()) return "";
  return spt.SerializeAsString();
}

std::string SentencePieceProcessor::DecodePiecesAsSerializedProto(
    const std::vector<std::string> &pieces) const {
  SentencePieceText spt;
  if (!Decode(pieces, &spt).ok()) return "";
  return spt.SerializeAsString();
}

std::string SentencePieceProcessor::DecodeIdsAsSerializedProto(
    const std::vector<int> &ids) const {
  SentencePieceText spt;
  if (!Decode(ids, &spt).ok()) return "";
  return spt.SerializeAsString();
}

#define CHECK_STATUS_OR_RETURN_DEFAULT(value)                                \
  if (!status().ok()) {                                                      \
    LOG(ERROR) << status().message() << "\nReturns default value " << value; \
    return value;                                                            \
  }

int SentencePieceProcessor::GetPieceSize() const {
  CHECK_STATUS_OR_RETURN_DEFAULT(0);
  return model_->GetPieceSize();
}

int SentencePieceProcessor::PieceToId(absl::string_view piece) const {
  CHECK_STATUS_OR_RETURN_DEFAULT(0);
  return model_->PieceToId(piece);
}

const std::string &SentencePieceProcessor::IdToPiece(int id) const {
  static const std::string *kEmptyString = new std::string;
  CHECK_STATUS_OR_RETURN_DEFAULT(*kEmptyString);
  return model_->IdToPiece(id);
}

float SentencePieceProcessor::GetScore(int id) const {
  CHECK_STATUS_OR_RETURN_DEFAULT(0.0);
  return model_->GetScore(id);
}

bool SentencePieceProcessor::IsControl(int id) const {
  CHECK_STATUS_OR_RETURN_DEFAULT(0);
  return model_->IsControl(id);
}

bool SentencePieceProcessor::IsUnknown(int id) const {
  CHECK_STATUS_OR_RETURN_DEFAULT(0);
  return model_->IsUnknown(id);
}

bool SentencePieceProcessor::IsUnused(int id) const {
  CHECK_STATUS_OR_RETURN_DEFAULT(false);
  return model_->IsUnused(id);
}

int SentencePieceProcessor::unk_id() const {
  const int id = PieceToId(absl::string_view(model_->unk_piece().data()));
  if (IsUnknown(id)) return id;
  return -1;
}

int SentencePieceProcessor::bos_id() const {
  const int id = PieceToId(absl::string_view(model_->bos_piece().data()));
  if (IsControl(id)) return id;
  return -1;
}

int SentencePieceProcessor::eos_id() const {
  const int id = PieceToId(absl::string_view(model_->eos_piece().data()));
  if (IsControl(id)) return id;
  return -1;
}

int SentencePieceProcessor::pad_id() const {
  const int id = PieceToId(absl::string_view(model_->pad_piece().data()));
  if (IsControl(id)) return id;
  return -1;
}

// static
::util::Status SentencePieceProcessor::ApplyExtraOptions(
    const std::vector<ExtraOption> &extra_options,
    SentencePieceText *spt) const {
  for (const auto &extra_option : extra_options) {
    switch (extra_option) {
      case REVERSE:
        std::reverse(spt->mutable_pieces()->begin(),
                     spt->mutable_pieces()->end());
        break;
      case EOS: {
        auto *piece = spt->add_pieces();
        piece->set_id(PieceToId(absl::string_view(model_->eos_piece().data())));
        piece->set_piece(model_->eos_piece().data(),
                         model_->eos_piece().size());
      } break;
      case BOS: {
        auto *array = spt->mutable_pieces();
        array->Add();
        for (int i = array->size() - 1; i > 0; --i) {
          array->SwapElements(i - 1, i);
        }
        auto *piece = array->Mutable(0);
        piece->set_id(PieceToId(absl::string_view(model_->bos_piece().data())));
        piece->set_piece(model_->bos_piece().data(),
                         model_->bos_piece().size());
      } break;
      default:
        return ::util::InternalError("unknown extra_option type.");
    }
  }

  return ::util::OkStatus();
}

// static
::util::Status SentencePieceProcessor::ParseExtraOptions(
    absl::string_view _extra_option,
    std::vector<SentencePieceProcessor::ExtraOption> *extra_options) const {
  absl::string_view extra_option(_extra_option.data(), _extra_option.size());

  extra_options->clear();
  if (extra_option.empty()) return ::util::OkStatus();

  RETURN_IF_ERROR(status());

  static std::map<absl::string_view, SentencePieceProcessor::ExtraOption>
      extra_option_map = {{"bos", SentencePieceProcessor::BOS},
                          {"eos", SentencePieceProcessor::EOS},
                          {"reverse", SentencePieceProcessor::REVERSE}};
  for (const auto &s : absl::StrSplit(extra_option, ":")) {
    const auto it = extra_option_map.find(s);
    CHECK_OR_RETURN(it != extra_option_map.end())
        << "option \"" << s << "\" is not available.";
    extra_options->push_back(it->second);

    if (it->second == SentencePieceProcessor::BOS) {
      CHECK_OR_RETURN(
          !IsUnknown(PieceToId(absl::string_view(model_->bos_piece().data()))))
          << "id for `" << model_->bos_piece() << "` is not defined.";
    }
    if (it->second == SentencePieceProcessor::EOS) {
      CHECK_OR_RETURN(
          !IsUnknown(PieceToId(absl::string_view(model_->eos_piece().data()))))
          << "id for `" << model_->eos_piece() << "` is not defined.";
    }
  }
  return ::util::OkStatus();
}

void SentencePieceProcessor::SetModel(std::unique_ptr<ModelInterface> &&model) {
  model_ = std::move(model);
}

void SentencePieceProcessor::SetNormalizer(
    std::unique_ptr<normalizer::Normalizer> &&normalizer) {
  normalizer_ = std::move(normalizer);
}

const ModelProto &SentencePieceProcessor::model_proto() const {
  return *model_proto_;
}

namespace io {

::util::Status LoadModelProto(absl::string_view filename,
                              ModelProto *model_proto) {
  if (filename.empty()) {
    return ::util::NotFoundError("model file path should not be empty.");
  }

  auto input = filesystem::NewReadableFile(filename, true);
  RETURN_IF_ERROR(input->status());
  std::string serialized;
  CHECK_OR_RETURN(input->ReadAll(&serialized));
  CHECK_OR_RETURN(
      model_proto->ParseFromArray(serialized.data(), serialized.size()));

  return ::util::OkStatus();
}

::util::Status SaveModelProto(absl::string_view filename,
                              const ModelProto &model_proto) {
  if (filename.empty()) {
    return ::util::NotFoundError("model file path should not be empty.");
  }
  auto output = filesystem::NewWritableFile(filename, true);
  RETURN_IF_ERROR(output->status());
  CHECK_OR_RETURN(output->Write(model_proto.SerializeAsString()));

  return ::util::OkStatus();
}
}  // namespace io
}  // namespace sentencepiece
