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

#include "sentencepiece_processor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "common.h"
#include "filesystem.h"
#include "model_factory.h"
#include "model_interface.h"
#include "normalizer.h"
#include "sentencepiece.pb.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/fixed_array.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "unigram_model.h"
#include "util.h"

#ifdef _USE_EXTERNAL_PROTOBUF
#include "google/protobuf/arena.h"
#else
#include "third_party/protobuf-lite/google/protobuf/arena.h"
#endif

using ::google::protobuf::Arena;

namespace sentencepiece {
namespace {

// Replaces white space with U+2581 (LOWER ONE EIGHT BLOCK).
constexpr absl::string_view kSpaceSymbol = "\xe2\x96\x81";

// Encodes <unk> into U+2047 (DOUBLE QUESTION MARK),
// since this character can be useful both for user and
// developer. We can easily figure out that <unk> is emitted.
constexpr absl::string_view kDefaultUnknownSymbol = " \xE2\x81\x87 ";

// REPLACEMENT CHARACTER (U+FFFD) in UTF-8.
constexpr absl::string_view kReplacementCharacter = "\xef\xbf\xbd";

// maximum nbest or sampling size.
constexpr int kMaxNBestSize = 512;

std::vector<absl::string_view> ToPieceArray(const std::vector<std::string>& v) {
  std::vector<absl::string_view> out(v.size());
  for (size_t i = 0; i < v.size(); ++i) out[i] = v[i];
  return out;
}

void ConvertToUnicodeSpansInternal(SentencePieceText* spt) {
  if (spt == nullptr || spt->text().empty()) return;

  std::vector<int> utf8_to_unicode(spt->text().size() + 1, 0);
  absl::string_view str = spt->text();
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

  auto clip = [&](int s) {
    return std::min<int>(std::max<int>(0, s), utf8_to_unicode.size() - 1);
  };

  for (auto& piece : *(spt->mutable_pieces())) {
    piece.set_begin(utf8_to_unicode[clip(piece.begin())]);
    piece.set_end(utf8_to_unicode[clip(piece.end())]);
  }
}

}  // namespace

ImmutableSentencePieceText::ImmutableSentencePieceText()
    : spt_(&SentencePieceText::default_instance()) {}

ImmutableSentencePieceText::ImmutableSentencePieceText(
    const SentencePieceText& spt)
    : spt_(&spt) {}

ImmutableSentencePieceText::~ImmutableSentencePieceText() {}

ImmutableSentencePieceText_ImmutableSentencePiece::
    ImmutableSentencePieceText_ImmutableSentencePiece()
    : sp_(&SentencePieceText_SentencePiece::default_instance()) {}

ImmutableSentencePieceText_ImmutableSentencePiece::
    ImmutableSentencePieceText_ImmutableSentencePiece(
        const SentencePieceText_SentencePiece& sp)
    : sp_(&sp) {}

const std::string& ImmutableSentencePieceText_ImmutableSentencePiece::piece()
    const {
  return sp_->piece();
}

const std::string& ImmutableSentencePieceText_ImmutableSentencePiece::surface()
    const {
  return sp_->surface();
}

uint32_t ImmutableSentencePieceText_ImmutableSentencePiece::id() const {
  return sp_->id();
}

uint32_t ImmutableSentencePieceText_ImmutableSentencePiece::begin() const {
  return sp_->begin();
}

uint32_t ImmutableSentencePieceText_ImmutableSentencePiece::end() const {
  return sp_->end();
}

std::vector<ImmutableSentencePieceText_ImmutableSentencePiece>
ImmutableSentencePieceText::pieces() const {
  std::vector<ImmutableSentencePieceText_ImmutableSentencePiece> pieces(
      spt_->pieces_size());
  for (int i = 0; i < spt_->pieces_size(); ++i)
    pieces[i] =
        ImmutableSentencePieceText_ImmutableSentencePiece(spt_->pieces(i));
  return pieces;
}

size_t ImmutableSentencePieceText::pieces_size() const {
  return spt_->pieces_size();
}

ImmutableSentencePieceText_ImmutableSentencePiece
ImmutableSentencePieceText::pieces(int index) const {
  return ImmutableSentencePieceText_ImmutableSentencePiece(spt_->pieces(index));
}

const std::string& ImmutableSentencePieceText::text() const {
  return spt_->text();
}

float ImmutableSentencePieceText::score() const {
  return spt_ ? spt_->score() : 0.0;
}

SentencePieceText* ImmutableSentencePieceText::mutable_proto() {
  if (rep_ == nullptr) {
    rep_ = std::make_shared<SentencePieceText>();
    spt_ = rep_.get();
  }
  return rep_.get();
}

void ImmutableSentencePieceText::ConvertToUnicodeSpans() {
  ConvertToUnicodeSpansInternal(mutable_proto());
}

util::bytes ImmutableSentencePieceText::SerializeAsString() const {
  return spt_->SerializeAsString();
}

ImmutableNBestSentencePieceText::ImmutableNBestSentencePieceText() {}
ImmutableNBestSentencePieceText::~ImmutableNBestSentencePieceText() {}

size_t ImmutableNBestSentencePieceText::nbests_size() const {
  return rep_ ? rep_->nbests_size() : 0;
}

ImmutableSentencePieceText ImmutableNBestSentencePieceText::nbests(
    int index) const {
  return ImmutableSentencePieceText(rep_->nbests(index));
}

std::vector<ImmutableSentencePieceText>
ImmutableNBestSentencePieceText::nbests() const {
  if (rep_ == nullptr) return {};
  std::vector<ImmutableSentencePieceText> nbests(rep_->nbests_size());
  for (int i = 0; i < rep_->nbests_size(); ++i)
    nbests[i] = ImmutableSentencePieceText(rep_->nbests(i));
  return nbests;
}

NBestSentencePieceText* ImmutableNBestSentencePieceText::mutable_proto() {
  if (rep_ == nullptr) {
    rep_ = std::make_shared<NBestSentencePieceText>();
  }
  return rep_.get();
}

void ImmutableNBestSentencePieceText::ConvertToUnicodeSpans() {
  if (!mutable_proto()) return;
  for (auto& spt : *(mutable_proto()->mutable_nbests())) {
    ConvertToUnicodeSpansInternal(&spt);
  }
}

util::bytes ImmutableNBestSentencePieceText::SerializeAsString() const {
  return rep_ ? rep_->SerializeAsString() : "";
}

SentencePieceProcessor::SentencePieceProcessor() {}
SentencePieceProcessor::~SentencePieceProcessor() {}

absl::Status SentencePieceProcessor::Load(absl::string_view filename) {
  auto model_proto = std::make_unique<ModelProto>();
  RETURN_IF_ERROR(io::LoadModelProto(filename, model_proto.get()));
  return Load(std::move(model_proto));
}

void SentencePieceProcessor::LoadOrDie(absl::string_view filename) {
  ABSL_CHECK_OK(Load(filename));
}

absl::Status SentencePieceProcessor::Load(const ModelProto& model_proto) {
  auto model_proto_copy = std::make_unique<ModelProto>();
  *model_proto_copy = model_proto;
  return Load(std::move(model_proto_copy));
}

absl::Status SentencePieceProcessor::LoadFromSerializedProto(
    absl::string_view serialized) {
  auto model_proto = std::make_unique<ModelProto>();
  RET_CHECK(model_proto->ParseFromArray(serialized.data(), serialized.size()));
  return Load(std::move(model_proto));
}

absl::Status SentencePieceProcessor::Load(
    std::unique_ptr<ModelProto> model_proto) {
  model_proto_ = std::move(model_proto);
  model_ = ModelFactory::Create(*model_proto_);
  normalizer_ = std::make_unique<normalizer::Normalizer>(
      model_proto_->normalizer_spec(), model_proto_->trainer_spec());
  if (model_proto_->has_denormalizer_spec() &&
      !model_proto_->denormalizer_spec().precompiled_charsmap().empty()) {
    denormalizer_ = std::make_unique<normalizer::Normalizer>(
        model_proto_->denormalizer_spec());
  }

  // Escapes user-defined-symbols in normalizer.
  normalizer_->SetPrefixMatcher(model_->prefix_matcher());

  RETURN_IF_ERROR(status());

  // Precomputes and caches special token IDs.
  // Note that these IDs are not always the same as the IDs in TrainerSpec.
  unk_id_ = PieceToId(model_->unk_piece());
  if (!IsUnknown(unk_id_)) unk_id_ = -1;

  bos_id_ = PieceToId(model_->bos_piece());
  if (!IsControl(bos_id_)) bos_id_ = -1;

  eos_id_ = PieceToId(model_->eos_piece());
  if (!IsControl(eos_id_)) eos_id_ = -1;

  pad_id_ = PieceToId(model_->pad_piece());
  if (!IsControl(pad_id_)) pad_id_ = -1;

  // Running self-testing.
  std::vector<std::string> errors, sps;
  for (const auto& s : model_proto_->self_test_data().samples()) {
    RETURN_IF_ERROR(Encode(s.input(), &sps));
    const std::string result = absl::StrJoin(sps, " ");
    if (!model_->VerifyOutputsEquivalent(s.expected(), result)) {
      errors.emplace_back(
          absl::StrCat(s.input(), "\t", s.expected(), "\t", result));
    }
  }

  if (!errors.empty()) {
    ABSL_LOG(INFO) << errors.size() << "/"
              << model_proto_->self_test_data().samples_size()
              << " samples did not pass the test.";
    for (const auto& e : errors) {
      ABSL_LOG(INFO) << e;
    }
    return absl::InternalError("Self-test failures. See ABSL_LOG(INFO).");
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SetEncodeExtraOptions(
    absl::string_view extra_options) {
  return ParseExtraOptions(extra_options, &encode_extra_options_);
}

absl::Status SentencePieceProcessor::SetDecodeExtraOptions(
    absl::string_view extra_options) {
  return ParseExtraOptions(extra_options, &decode_extra_options_);
}

absl::Status SentencePieceProcessor::status() const {
  RET_CHECK(model_) << "Model is not initialized.";
  RET_CHECK(normalizer_) << "Normalizer is not initialized.";
  RETURN_IF_ERROR(model_->status());
  RETURN_IF_ERROR(normalizer_->status());
  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SetVocabulary(
    const std::vector<absl::string_view>& valid_vocab) {
  ABSL_LOG(WARNING) << "SetVocabulary will be deprecated in v0.2.3";
  RETURN_IF_ERROR(status());

  // TODO(taku): supports vocabulary constraint in BPE model.
  const auto type = model_proto_->trainer_spec().model_type();
  RET_CHECK(type == TrainerSpec::UNIGRAM || type == TrainerSpec::BPE)
      << "Vocabulary constraint is only enabled in subword units.";

  const absl::flat_hash_set<absl::string_view> vocab(valid_vocab.begin(),
                                                     valid_vocab.end());

  for (int i = 0; i < model_proto_->pieces_size(); ++i) {
    auto* piece = model_proto_->mutable_pieces(i);
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

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::ResetVocabulary() {
  ABSL_LOG(WARNING) << "ResetVocabulary will be deprecated in v0.2.3";
  RETURN_IF_ERROR(status());
  for (auto& piece : *(model_proto_->mutable_pieces())) {
    if (piece.type() == ModelProto::SentencePiece::UNUSED)
      piece.set_type(ModelProto::SentencePiece::NORMAL);
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::LoadVocabulary(absl::string_view filename,
                                                    int threshold) {
  ABSL_LOG(WARNING) << "LoadVocabulary will be deprecated in v0.2.3";
  auto input = filesystem::NewReadableFile(filename);
  RETURN_IF_ERROR(input->status());

  std::string line;
  std::vector<std::string> vocab;

  while (input->ReadLine(&line)) {
    const std::vector<std::string> v = absl::StrSplit(line, '\t');
    RET_CHECK_GE(v.size(), 1);
    RET_CHECK(!v[0].empty());
    int32_t freq = 1;
    if (v.size() >= 2) {
      RET_CHECK(absl::SimpleAtoi(v[1], &freq))
          << "Could not parse the frequency";
    }
    if (freq >= threshold) {
      vocab.emplace_back(v[0]);
    }
  }

  return SetVocabulary(ToPieceArray(vocab));
}

#define RET_CHECK_STATUS_STL(container)               \
  RETURN_IF_ERROR(status());                          \
  RET_CHECK(container) << "output container is null"; \
  container->clear();

#define RET_CHECK_STATUS_PROTO(proto)         \
  RETURN_IF_ERROR(status());                  \
  RET_CHECK(proto) << "output proto is null"; \
  proto->Clear();

//////////////////////////////////////////////////////////////
// Simple API.
absl::Status SentencePieceProcessor::Encode(
    absl::string_view input, std::vector<std::string>* pieces) const {
  return EncodeOptimized(input, pieces);
}

absl::Status SentencePieceProcessor::Encode(absl::string_view input,
                                            std::vector<int>* ids) const {
  return EncodeOptimized(input, ids);
}

absl::Status SentencePieceProcessor::Decode(
    absl::Span<const std::string> pieces, std::string* detokenized) const {
  absl::FixedArray<absl::string_view, 128> views(pieces.begin(), pieces.end());
  return DecodeOptimized<absl::string_view>(views, detokenized);
}

absl::Status SentencePieceProcessor::Decode(
    absl::Span<const absl::string_view> pieces,
    std::string* detokenized) const {
  return DecodeOptimized(pieces, detokenized);
}

absl::Status SentencePieceProcessor::Decode(absl::Span<const int> ids,
                                            std::string* detokenized) const {
  return DecodeOptimized(ids, detokenized);
}

absl::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    std::vector<std::vector<std::string>>* pieces) const {
  RET_CHECK_STATUS_STL(pieces);

  Arena arena;
  auto* spt = Arena::Create<NBestSentencePieceText>(&arena);
  RETURN_IF_ERROR(NBestEncode(input, nbest_size, spt));
  pieces->reserve(spt->nbests().size());
  for (const auto& nbest : spt->nbests()) {
    std::vector<std::string>& result = pieces->emplace_back();
    result.reserve(nbest.pieces().size());
    for (const auto& sp : nbest.pieces()) {
      result.emplace_back(sp.piece());
    }
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    std::vector<std::vector<int>>* ids) const {
  RET_CHECK_STATUS_STL(ids);

  Arena arena;
  auto* spt = Arena::Create<NBestSentencePieceText>(&arena);
  RETURN_IF_ERROR(NBestEncode(input, nbest_size, spt));
  ids->reserve(spt->nbests().size());
  for (const auto& nbest : spt->nbests()) {
    std::vector<int>& result = ids->emplace_back();
    result.reserve(nbest.pieces().size());
    for (const auto& sp : nbest.pieces()) {
      result.emplace_back(sp.id());
    }
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncode(
    absl::string_view input, int nbest_size, float alpha,
    std::vector<std::string>* pieces) const {
  RET_CHECK_STATUS_STL(pieces);

  Arena arena;
  auto* spt = Arena::Create<SentencePieceText>(&arena);
  RETURN_IF_ERROR(SampleEncode(input, nbest_size, alpha, spt));
  pieces->reserve(spt->pieces().size());
  for (const auto& sp : spt->pieces()) {
    pieces->emplace_back(sp.piece());
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncode(absl::string_view input,
                                                  int nbest_size, float alpha,
                                                  std::vector<int>* ids) const {
  RET_CHECK_STATUS_STL(ids);

  Arena arena;
  auto* spt = Arena::Create<SentencePieceText>(&arena);
  RETURN_IF_ERROR(SampleEncode(input, nbest_size, alpha, spt));
  for (const auto& sp : spt->pieces()) {
    ids->emplace_back(sp.id());
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncodeAndScore(
    absl::string_view input, int num_samples, float alpha, bool wor,
    bool include_best,
    std::vector<std::pair<std::vector<std::string>, float>>* pieces) const {
  RET_CHECK_STATUS_STL(pieces);

  Arena arena;
  auto* spt = Arena::Create<NBestSentencePieceText>(&arena);
  RETURN_IF_ERROR(
      SampleEncodeAndScore(input, num_samples, alpha, wor, include_best, spt));

  pieces->clear();
  pieces->reserve(spt->nbests_size());

  for (const auto& nbest : spt->nbests()) {
    std::vector<std::string> result;
    result.reserve(nbest.pieces_size());
    for (const auto& sp : nbest.pieces()) {
      result.emplace_back(sp.piece());
    }
    pieces->emplace_back(std::move(result), nbest.score());
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncodeAndScore(
    absl::string_view input, int num_samples, float alpha, bool wor,
    bool include_best,
    std::vector<std::pair<std::vector<int>, float>>* ids) const {
  RET_CHECK_STATUS_STL(ids);

  Arena arena;
  auto* spt = Arena::Create<NBestSentencePieceText>(&arena);
  RETURN_IF_ERROR(
      SampleEncodeAndScore(input, num_samples, alpha, wor, include_best, spt));

  ids->clear();
  ids->reserve(spt->nbests_size());

  for (const auto& nbest : spt->nbests()) {
    std::vector<int> result;
    result.reserve(nbest.pieces_size());
    for (const auto& sp : nbest.pieces()) {
      result.emplace_back(sp.id());
    }
    ids->emplace_back(std::move(result), nbest.score());
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::PopulateSentencePieceText(
    absl::string_view input, absl::string_view normalized,
    absl::Span<const size_t> norm_to_orig, const EncodeResult& result,
    SentencePieceText* spt, bool skip_surface,
    size_t input_start_offset) const {
  size_t consumed = 0;
  bool is_prev_unk = false;
  for (const auto& p : result) {
    const absl::string_view w = p.first;  // piece
    const int id = p.second;              // id

    RET_CHECK(!w.empty()) << "Empty piece is not allowed.";

    const bool is_unk = IsUnknown(id);

    if (IsControl(id)) {
      // Control symbol has no corresponding source surface, so begin == end.
      auto* sp = spt->add_pieces();
      sp->set_piece(w.data(), w.size());
      sp->set_id(id);
      RET_CHECK_GE(norm_to_orig[consumed], input_start_offset);
      const size_t orig_offset = norm_to_orig[consumed] - input_start_offset;
      sp->set_begin(orig_offset);
      sp->set_end(orig_offset);
    } else {
      const size_t begin = consumed;
      const size_t end = consumed + w.size();
      RET_CHECK_LT(begin, norm_to_orig.size());
      RET_CHECK_LT(end, norm_to_orig.size());
      RET_CHECK_GE(norm_to_orig[begin], input_start_offset);
      RET_CHECK_GE(norm_to_orig[end], input_start_offset);
      const size_t orig_begin = norm_to_orig[begin] - input_start_offset;
      const size_t orig_end = norm_to_orig[end] - input_start_offset;
      RET_CHECK_LE(orig_begin, input.size());
      RET_CHECK_LE(orig_end, input.size());
      RET_CHECK_LE(orig_begin, orig_end);
      const auto surface =
          absl::ClippedSubstr(input, orig_begin, orig_end - orig_begin);

      if (is_unk && model_->ByteFallbackEnabled()) {
        // Decomposes an unknown piece into UTF-8 bytes
        for (size_t i = 0; i < w.size(); ++i) {
          // Create a byte piece
          const uint8_t b = static_cast<uint8_t>(w[i]);
          SentencePieceText::SentencePiece* sp = spt->add_pieces();
          std::string& piece = *sp->mutable_piece();
          piece = ByteToPiece(b);
          int sp_id = model_->PieceToId(piece);
          sp->set_id(sp_id);

          // The last byte piece holds the surface of the original unknown
          // character. The other byte pieces have no surface.
          if (i == w.size() - 1) {
            if (!skip_surface) sp->set_surface(surface.data(), surface.size());
            sp->set_begin(orig_begin);
            sp->set_end(orig_end);
          } else {
            // begin == end
            sp->set_begin(orig_begin);
            sp->set_end(orig_begin);
          }
        }
      } else {
        // Merges continuous run of unknown pieces so that decoder
        // can copy or generate unknown tokens easily.
        // Note that merged tokens are still unknown,
        // since known pieces never consist of unknown characters.
        if (is_prev_unk && is_unk) {
          auto* sp = spt->mutable_pieces(spt->pieces_size() - 1);
          sp->mutable_piece()->append(w);
          if (!skip_surface) sp->mutable_surface()->append(surface);
          sp->set_end(orig_end);
        } else {
          auto* sp = spt->add_pieces();
          sp->set_piece(w.data(), w.size());
          sp->set_id(id);
          if (!skip_surface) sp->set_surface(surface.data(), surface.size());
          sp->set_begin(orig_begin);
          sp->set_end(orig_end);
        }
      }
      consumed += w.size();
    }
    is_prev_unk = is_unk;
  }

  RET_CHECK_EQ(consumed, normalized.size())
      << "all normalized characters are not consumed.";

  spt->set_text(input.data(), input.size());

  RETURN_IF_ERROR(ApplyExtraOptions(encode_extra_options_, spt));

  return absl::OkStatus();
}  // namespace sentencepiece

absl::Status SentencePieceProcessor::Encode(absl::string_view input,
                                            SentencePieceText* spt) const {
  RET_CHECK_STATUS_PROTO(spt);

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  const auto result = model_->Encode(normalized);
  RETURN_IF_ERROR(
      PopulateSentencePieceText(input, normalized, norm_to_orig, result, spt));

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::NBestEncode(
    absl::string_view input, int nbest_size,
    NBestSentencePieceText* nbest_spt) const {
  RET_CHECK_STATUS_PROTO(nbest_spt);

  RET_CHECK_LE(nbest_size, kMaxNBestSize)
      << "nbest_size must be nbest_size <= " << kMaxNBestSize;

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  RET_CHECK(model_->IsNBestEncodeAvailable())
      << "NBestEncode is not available for the current model.";

  const auto nbests = model_->NBestEncode(normalized, nbest_size);
  RET_CHECK(!nbests.empty()) << "NBestEncode returns empty result.";

  for (const auto& result : nbests) {
    auto* spt = nbest_spt->add_nbests();
    spt->set_score(result.second);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result.first, spt));
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncode(
    absl::string_view input, int nbest_size, float alpha,
    SentencePieceText* spt) const {
  RET_CHECK_STATUS_PROTO(spt);

  RET_CHECK(!std::isnan(alpha));
  RET_CHECK_GE(alpha, 0.0);
  RET_CHECK_LE(nbest_size, kMaxNBestSize)
      << "nbest_size must be nbest_size <= " << kMaxNBestSize;

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  if (!model_->IsNBestEncodeAvailable() || nbest_size < 0) {
    RET_CHECK(model_->IsSampleEncodeAvailable())
        << "SampleEncode is not available for the current model.";
    const auto result = model_->SampleEncode(normalized, alpha);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result, spt));
  } else if (nbest_size == 1 || nbest_size == 0) {
    const auto result = model_->Encode(normalized);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result, spt));
  } else if (nbest_size > 1) {
    const auto nbests = model_->NBestEncode(normalized, nbest_size);
    RET_CHECK(!nbests.empty()) << "NBestEncode returns empty result.";

    std::vector<double> log_probs;
    log_probs.reserve(nbests.size());
    std::transform(nbests.begin(), nbests.end(), std::back_inserter(log_probs),
                   [alpha](const auto& nbest) { return alpha * nbest.second; });

    const double Z = log_domain::LogSum(log_probs);
    std::vector<double> probs;
    probs.reserve(log_probs.size());
    std::transform(
        log_probs.begin(), log_probs.end(), std::back_inserter(probs),
        [Z](const auto& log_prob) { return std::exp(log_prob - Z); });

    auto* mt = random::GetRandomGenerator();
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              nbests[dist(*mt)].first, spt));
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::SampleEncodeAndScore(
    absl::string_view input, int samples, float alpha, bool wor,
    bool include_best, NBestSentencePieceText* samples_spt) const {
  RET_CHECK(!std::isnan(alpha));
  RET_CHECK_GE(alpha, 0.0);
  RET_CHECK_GE(samples, 0);
  RET_CHECK_LE(samples, kMaxNBestSize)
      << "samples must be nbest_size <= " << kMaxNBestSize;

  RET_CHECK(model_->IsSampleEncodeAndScoreAvailable())
      << "SampleEncodeAndScore is not available for the current model.";
  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  const auto results = model_->SampleEncodeAndScore(normalized, alpha, samples,
                                                    wor, include_best);
  RET_CHECK(!results.empty()) << "SampleEncodeAndScore returns empty result.";

  for (const auto& result : results) {
    auto* spt = samples_spt->add_nbests();
    spt->set_score(result.second);
    RETURN_IF_ERROR(PopulateSentencePieceText(input, normalized, norm_to_orig,
                                              result.first, spt));
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::CalculateEntropy(absl::string_view input,
                                                      float alpha,
                                                      float* entropy) const {
  RET_CHECK(!std::isnan(alpha));
  RET_CHECK_GE(alpha, 0.0);
  RET_CHECK(model_->IsCalculateEntropyAvailable())
      << "CalculateEntropy is not available for the current model.";
  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  *entropy = model_->CalculateEntropy(normalized, alpha);
  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::Decode(
    absl::Span<const std::string> pieces, SentencePieceText* spt) const {
  absl::FixedArray<absl::string_view, 128> views(pieces.begin(), pieces.end());
  return Decode(views, spt);
}

absl::Status SentencePieceProcessor::Decode(
    absl::Span<const absl::string_view> pieces, SentencePieceText* spt) const {
  RET_CHECK_STATUS_PROTO(spt);

  absl::string_view unk_surface = kDefaultUnknownSymbol;
  if (model_proto_ && model_proto_->trainer_spec().has_unk_surface())
    unk_surface = model_proto_->trainer_spec().unk_surface();

  // Returns decoded piece and a boolean indicating if the function has consumed
  // a bos whitespace token (a piece starting with a kSpaceSymbol). This is used
  // to strip only the first whitespace token from the decoded sequence for
  // add_dummy_prefix.
  auto DecodeSentencePiece =
      [&](absl::string_view piece, int id,
          bool is_bos_ws) -> std::pair<std::string, bool> {
    if (IsControl(id)) {                 // <s>, </s>
      return std::make_pair("", false);  // invisible symbol.
    } else if (IsUnknown(id)) {
      if (IdToPiece(id) == piece) {  // <unk>
        return std::make_pair(std::string(unk_surface), false);
      } else {  // return piece when piece is not <unk>.
        return std::make_pair(std::string(piece), false);
      }
    }

    bool has_bos_ws = false;  // whether the token starts with a kSpaceSymbol
    if (is_bos_ws &&
        (!model_proto_ ||
         (model_proto_ &&
          (model_proto_->normalizer_spec().add_dummy_prefix() ||
           model_proto_->normalizer_spec().remove_extra_whitespaces())))) {
      // Consume if the current position is bos and
      // piece starts with kSpaceSymbol.
      has_bos_ws = absl::ConsumePrefix(&piece, kSpaceSymbol);

      if (model_proto_ &&
          model_proto_->normalizer_spec().remove_extra_whitespaces()) {
        // if we are removing extra whitespace, we remove all leading whitespace
        has_bos_ws = false;
      }
    }

    return std::make_pair(absl::StrReplaceAll(piece, {{kSpaceSymbol, " "}}),
                          has_bos_ws);
  };

  spt->mutable_pieces()->Reserve(pieces.size());
  for (absl::string_view w : pieces) {
    auto* sp = spt->add_pieces();
    sp->mutable_piece()->assign(w.data(), w.size());
    sp->set_id(PieceToId(w));
  }

  RETURN_IF_ERROR(ApplyExtraOptions(decode_extra_options_, spt));

  std::string* text = spt->mutable_text();
  auto SetSurface = [&](int index, absl::string_view surface) {
    auto* sp = spt->mutable_pieces(index);
    sp->set_surface(surface.data(), surface.size());
    sp->set_begin(text->size());
    sp->set_end(text->size() + surface.size());
    absl::StrAppend(text, surface);
  };

  auto ProcessBytePieces = [&](int token_index_begin,
                               int token_index_end) -> absl::Status {
    if (token_index_begin >= token_index_end) {
      return absl::OkStatus();
    }

    // Constructs byte sequence.
    std::string bytes;
    for (int i = token_index_begin; i < token_index_end; ++i) {
      const auto& sp = spt->pieces(i);
      const int byte = PieceToByte(sp.piece());
      RET_CHECK_LE(0, byte);
      bytes.append(1, byte);
    }

    // Set surfaces of `bytes` for each Unicode character.
    int offset = 0;
    const int bytes_len = bytes.size();
    while (offset < bytes_len) {
      // Consume `bytes` by one Unicode character.
      size_t consumed;  // Number of bytes consumed in this iteration.
      const bool is_valid = string_util::IsValidDecodeUTF8(
          absl::string_view(bytes).substr(offset), &consumed);

      // Set surfaces of the consumed byte pieces.
      const int token_index = token_index_begin + offset;

      if (!is_valid) {
        // The byte piece at `token_index` is structurally invalid. Map it to
        // REPLACEMENT CHARACTER (U+FFFD).
        RET_CHECK_EQ(consumed, 1);
        SetSurface(token_index, kReplacementCharacter);
      } else {
        const absl::string_view utf8 =
            absl::string_view(bytes).substr(offset, consumed);
        for (size_t j = 0; j < consumed; j++) {
          // The last byte piece holds the surface of the original unknown
          // character. The other byte pieces hold an empty string as
          // surface.
          if (j == consumed - 1) {
            SetSurface(token_index + j, utf8);
          } else {
            SetSurface(token_index + j, "");
          }
        }
      }
      offset += consumed;
    }
    RET_CHECK_EQ(token_index_begin + offset, token_index_end);

    return absl::OkStatus();
  };

  int byte_start = 0;
  bool is_bos_ws = true;  // whether we expect a bos ws token to consume.
  bool bos_ws_seen = false;
  std::string decoded;

  for (int i = 0; i < spt->pieces_size(); ++i) {
    const auto& sp = spt->pieces(i);
    if (!IsByte(sp.id())) {
      RETURN_IF_ERROR(ProcessBytePieces(byte_start, i));

      // if we have seen a bos_ws token or any non-empty token
      if (bos_ws_seen || !text->empty()) is_bos_ws = false;

      byte_start = i + 1;
      std::tie(decoded, bos_ws_seen) =
          DecodeSentencePiece(sp.piece(), sp.id(), is_bos_ws);

      SetSurface(i, decoded);
    }
  }
  RETURN_IF_ERROR(ProcessBytePieces(byte_start, spt->pieces_size()));

  if (denormalizer_) {
    *text = denormalizer_->Normalize(*text);
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::Decode(absl::Span<const int> ids,
                                            SentencePieceText* spt) const {
  std::vector<absl::string_view> pieces;
  const int num_pieces = GetPieceSize();
  pieces.reserve(ids.size());
  for (const int id : ids) {
    if (id < 0 || id >= num_pieces) {
      return absl::Status(absl::StatusCode::kOutOfRange,
                          absl::StrCat("Invalid id: ", id));
    }
    pieces.emplace_back(IdToPiece(id));
  }
  return Decode(pieces, spt);
}

namespace {

void GetIthChunkBoundaries(
    const string_util::UnicodeText& input_unicode,
    const std::vector<uint32_t>& utf8_offsets, const size_t chunk_len,
    const size_t i, const size_t overlap,
    std::tuple<size_t, size_t, size_t>& chunk_boundaries) {
  const size_t unicode_start_offset = i * chunk_len;
  const size_t unicode_end_offset =
      std::min<size_t>((i + 1) * chunk_len, input_unicode.size());
  const size_t unicode_overlap_offset =
      std::min<size_t>((i + 1) * chunk_len + overlap, input_unicode.size());
  const size_t start_index = utf8_offsets[unicode_start_offset];
  const size_t end_index = utf8_offsets[unicode_end_offset];
  const size_t overlap_index = utf8_offsets[unicode_overlap_offset];
  chunk_boundaries = {start_index, end_index, overlap_index};
}

void GetChunkOfInput(const absl::string_view input,
                     const absl::string_view normalized,
                     absl::Span<const size_t> norm_to_orig,
                     const std::tuple<size_t, size_t, size_t>& chunk_boundaries,
                     absl::string_view& input_chunk,
                     absl::string_view& normalized_chunk,
                     absl::Span<const size_t>& norm_to_orig_chunk) {
  const size_t start_index = std::get<0>(chunk_boundaries);
  const size_t overlap_index = std::get<2>(chunk_boundaries);
  input_chunk = input.substr(start_index, overlap_index - start_index);

  auto normalized_start_it =
      std::lower_bound(norm_to_orig.begin(), norm_to_orig.end(), start_index);
  const int normalized_start_index =
      (normalized_start_it - norm_to_orig.begin());

  auto normalized_end_it =
      std::upper_bound(normalized_start_it, norm_to_orig.end(), overlap_index);
  const int normalized_end_index =
      (normalized_end_it - norm_to_orig.begin()) - 1;
  normalized_chunk =
      absl::ClippedSubstr(normalized, normalized_start_index,
                          normalized_end_index - normalized_start_index);
  const size_t slice_start =
      std::min<size_t>(normalized_start_index, norm_to_orig.size());
  const size_t slice_end =
      std::min<size_t>(normalized_end_index + 1, norm_to_orig.size());
  norm_to_orig_chunk =
      norm_to_orig.subspan(slice_start, slice_end - slice_start);
}

bool FindMatchingToken(const SentencePieceText_SentencePiece& current_piece,
                       const SentencePieceText& next_chunk, size_t chunk_len,
                       int unk_id, size_t* start_index_in_next_chunk) {
  bool found_matching_token = false;

  // Two tokens match if:
  //   - They have the same surface form and same start and end indices.
  //   - They are both UNK and overlap.
  // Note that we only match non zero-width tokens, to avoid matching dummy WS
  // and byte tokens.
  for (const auto& other_piece : next_chunk.pieces()) {
    if ((other_piece.id() == current_piece.id() &&
         other_piece.begin() + chunk_len == current_piece.begin() &&
         other_piece.end() + chunk_len == current_piece.end()) ||
        (current_piece.id() == static_cast<uint32_t>(unk_id) &&
         other_piece.id() == static_cast<uint32_t>(unk_id) &&
         other_piece.begin() <= current_piece.end() - chunk_len &&
         current_piece.end() - chunk_len <= other_piece.end())) {
      if (current_piece.begin() != current_piece.end()) {
        found_matching_token = true;
        *start_index_in_next_chunk = other_piece.end();
        break;
      } else if (other_piece.begin() + chunk_len > current_piece.end()) {
        // No potential matching piece. Terminate the search.
        break;
      }
    }
  }

  return found_matching_token;
}

ABSL_ATTRIBUTE_COLD absl::Status ReparseBadRanges(
    const std::vector<size_t>& bad_joins, const absl::string_view input,
    const absl::string_view normalized, absl::Span<const size_t> norm_to_orig,
    Arena& arena, const ModelInterface& model,
    absl::FunctionRef<absl::Status(
        absl::string_view input, absl::string_view normalized,
        absl::Span<const size_t> norm_to_orig, const EncodeResult& result,
        SentencePieceText* spt, size_t input_start_offset)>
        populate_sentence_piece_text,
    std::vector<SentencePieceText*>& spt_chunks,
    std::vector<std::tuple<size_t, size_t, size_t>>& input_chunk_boundaries) {
  // Convert bad joins into bad join ranges.
  // A bad join range is a contiguous sequence of chunks where none of
  // them joined together successfully.
  std::vector<std::pair<size_t, size_t>> bad_join_ranges;
  for (size_t i : bad_joins) {
    // Error at i means that i-1 and i don't work together.
    if (bad_join_ranges.empty() || bad_join_ranges.back().second < i) {
      bad_join_ranges.push_back(std::make_pair(i, i + 1));
    } else {
      bad_join_ranges.back().second = i + 1;
    }
  }

  // This means that we need to reparse the whole string.
  if (bad_joins.empty()) {
    bad_join_ranges.emplace_back(0, spt_chunks.size() - 1);
  }

  // For each bad range, we're going to merge the text and re-encode it.
  std::vector<SentencePieceText*> new_spt_chunks;
  std::vector<std::tuple<size_t, size_t, size_t>> new_boundaries_list;
  size_t old_index = 0;
  for (const auto& range : bad_join_ranges) {
    // Copy over things that are unchanged.
    for (; old_index < range.first; ++old_index) {
      new_spt_chunks.push_back(spt_chunks[old_index]);
      new_boundaries_list.push_back(input_chunk_boundaries[old_index]);
    }

    // Create new boundaries that encompass all of the chunks in the bad
    // joins range.
    const size_t first_chunk_start =
        std::get<0>(input_chunk_boundaries[range.first]);
    const size_t last_chunk_end =
        std::get<1>(input_chunk_boundaries[range.second]);
    const size_t last_chunk_overlap =
        std::get<2>(input_chunk_boundaries[range.second]);
    std::tuple<size_t, size_t, size_t> new_boundaries = {
        first_chunk_start, last_chunk_end, last_chunk_overlap};

    absl::string_view input_chunk;
    absl::string_view normalized_chunk;
    absl::Span<const size_t> norm_to_orig_chunk;

    // Fetch text for this new larger chunk.
    GetChunkOfInput(input, normalized, norm_to_orig, new_boundaries,
                    input_chunk, normalized_chunk, norm_to_orig_chunk);
    auto encode_result = model.Encode(normalized_chunk);
    auto* new_chunk = arena.Create<SentencePieceText>(&arena);
    RETURN_IF_ERROR(populate_sentence_piece_text(
        input_chunk, normalized_chunk, norm_to_orig_chunk, encode_result,
        new_chunk, std::get<0>(new_boundaries)));

    // Insert the new chunk and new boundaries.
    new_spt_chunks.push_back(new_chunk);
    new_boundaries_list.push_back(new_boundaries);
    // Skip forward past all of the chunks we just merged.
    old_index = range.second + 1;
  }

  // Add any remaining chunks from after the last bad range.
  for (; old_index < spt_chunks.size(); ++old_index) {
    new_spt_chunks.push_back(spt_chunks[old_index]);
    new_boundaries_list.push_back(input_chunk_boundaries[old_index]);
  }

  // Update spt_chunks and boundaries to the new merged list.
  spt_chunks = std::move(new_spt_chunks);
  input_chunk_boundaries = std::move(new_boundaries_list);

  return absl::OkStatus();
}

}  // namespace

absl::Status SentencePieceProcessor::ParallelEncodeInternal(
    absl::string_view input, size_t chunk_len, ThreadPool& thread_pool,
    std::vector<std::string>* pieces, std::vector<int>* ids,
    SentencePieceText* spt) const {
  if (input.empty()) {
    if (spt != nullptr) {
      spt->Clear();
      spt->set_text("");
    }
    return absl::OkStatus();
  }

  if (input.size() > std::numeric_limits<uint32_t>::max()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Input larger than ", std::numeric_limits<uint32_t>::max(),
                     " bytes is not supported."));
  }

  string_util::UnicodeTextAndOffsets unicode_text_and_offset =
      string_util::UTF8ToUnicodeTextAndOffsets(input);
  const string_util::UnicodeText& input_unicode =
      unicode_text_and_offset.unicode_text;
  const std::vector<uint32_t>& utf8_offsets = unicode_text_and_offset.offsets;

  std::string normalized;
  std::vector<size_t> norm_to_orig;
  RETURN_IF_ERROR(normalizer_->Normalize(input, &normalized, &norm_to_orig));

  // Set the overlap to be 2x the maximum piece length.
  size_t overlap = model_proto().trainer_spec().max_sentencepiece_length() * 2;
  size_t num_chunks = (input_unicode.size() + chunk_len - 1) / chunk_len;

  // Split unnormalized input into chunks, and then map those to chunks in the
  // normalized text.
  // Boundaries are (start, end, overlap); all are indices into the
  // utf8_offsets vector.
  std::vector<std::tuple<size_t, size_t, size_t>> input_chunk_boundaries(
      num_chunks);

  std::vector<SentencePieceText*> spt_chunks;
  spt_chunks.resize(num_chunks);
  Arena arena;

  // Create thread-local arenas to avoid lock contention on the main arena
  // during parallel encoding.
  std::vector<std::unique_ptr<Arena>> thread_arenas(thread_pool.num_threads());
  for (auto& thread_arena : thread_arenas) {
    thread_arena = std::make_unique<Arena>();
  }

  const bool create_own_spt = spt == nullptr;
  if (create_own_spt) {
    spt = arena.Create<SentencePieceText>(&arena);
  }
  absl::Cleanup cleanup = [create_own_spt, &spt] {
    if (create_own_spt) {
      spt = nullptr;
    }
  };

  {
    absl::BlockingCounter barrier(thread_pool.num_threads());
    absl::Mutex status_mutex;
    absl::Status encoding_status;
    for (size_t n = 0; n < thread_pool.num_threads(); ++n) {
      thread_pool.Schedule([&, n]() {
        Arena* thread_arena = thread_arenas[n].get();
        for (size_t i = n; i < num_chunks; i += thread_pool.num_threads()) {
          absl::string_view input_chunk;
          absl::string_view normalized_chunk;
          absl::Span<const size_t> norm_to_orig_chunk;
          GetIthChunkBoundaries(input_unicode, utf8_offsets, chunk_len, i,
                                overlap, input_chunk_boundaries[i]);
          GetChunkOfInput(input, normalized, norm_to_orig,
                          input_chunk_boundaries[i], input_chunk,
                          normalized_chunk, norm_to_orig_chunk);

          auto encode_result = model_->Encode(normalized_chunk);
          spt_chunks[i] = thread_arena->Create<SentencePieceText>(thread_arena);
          auto status = PopulateSentencePieceText(
              input_chunk, normalized_chunk, norm_to_orig_chunk, encode_result,
              spt_chunks[i], /*skip_surface=*/true,
              std::get<0>(input_chunk_boundaries[i]));
          // Can be optimized to cancel all other threads but then it would be
          // better to switch to absl::StatusBundle..
          if (!status.ok()) {
            absl::MutexLock lock(status_mutex);
            encoding_status = status;
          }
        }
        barrier.DecrementCount();
      });
    }

    barrier.Wait();
    // Note: This needs to be after the barrier.Wait() call resolves, since the
    // threads may still be running (and updating the status).
    RETURN_IF_ERROR(encoding_status);
  }

  // Now stitch the chunks together.
  // Given two consecutive chunks A and B, we want to find the first token in A
  // that is also a full token in B. At this point, we terminate iterating over
  // chunk A and start iterating over chunk B, starting at this known good
  // position.
  for (int loops = 0;; ++loops) {
    size_t start_of_good_tokens = 0;
    std::vector<size_t> bad_joins;
    spt->clear_pieces();
    if (pieces != nullptr) pieces->clear();
    if (ids != nullptr) ids->clear();
    for (size_t i = 0; i < spt_chunks.size(); ++i) {
      const size_t this_chunk_len = (std::get<1>(input_chunk_boundaries[i]) -
                                     std::get<0>(input_chunk_boundaries[i]));
      bool found_matching_token = false;
      for (const auto& piece : spt_chunks[i]->pieces()) {
        // Ignore tokens at the truncated beginning or pieces that don't
        // correspond to anything in the surface (these are often dummy WS).
        if (piece.begin() < start_of_good_tokens ||
            // This checks for dummy WS at the start of a piece.
            // We don't match 0 width byte pieces so this is fine.
            (i > 0 && piece.begin() == 0 && piece.begin() == piece.end())) {
          continue;
        }

        // Add pieces from the chunk. If the piece ends in the overlap, check
        // for a match in the next chunk.
        if (pieces != nullptr) pieces->emplace_back(piece.piece());
        if (ids != nullptr) ids->emplace_back(piece.id());
        auto sp = spt->add_pieces();
        sp->set_piece(piece.piece());
        sp->set_id(piece.id());
        sp->set_begin(piece.begin() + std::get<0>(input_chunk_boundaries[i]));
        sp->set_end(piece.end() + std::get<0>(input_chunk_boundaries[i]));
        // Reconstruct the surface string for the stitched piece.
        // - Control characters do not have a surface.
        // - For byte fallback pieces (IsByte is true), only the last piece in
        //   the sequence (where begin != end) gets the surface of the original
        //   unknown character. Intermediate byte pieces (begin == end) do not.
        // - Normal pieces (including dummy prefix space) always get their
        // surface.
        if (!IsControl(piece.id())) {
          if (!IsByte(piece.id()) || sp->begin() != sp->end()) {
            auto tmp = input.substr(sp->begin(), sp->end() - sp->begin());
            sp->set_surface(tmp.data(), tmp.size());
          }
        }

        // Try to find a matching token in the next chunk for this token.
        // Don't match zero width pieces.
        if (i < spt_chunks.size() - 1 && piece.begin() != piece.end() &&
            piece.end() >= this_chunk_len) {
          found_matching_token =
              FindMatchingToken(piece, *spt_chunks[i + 1], this_chunk_len,
                                unk_id(), &start_of_good_tokens);
          if (found_matching_token) {
            break;
          }
        }
      }
      if (!(found_matching_token || i == spt_chunks.size() - 1)) {
        // These two chunks failed to merge together. We'll fix this later.
        bad_joins.push_back(i);
      }
    }

    // If all join results are good, we're done.
    if (bad_joins.empty()) {
      break;
    }

    constexpr int kMaxReparseLoops = 3;

    if (loops >= kMaxReparseLoops) {
      // This tells ReparseBadRanges to parse the whole string.
      bad_joins.clear();
    }

    RETURN_IF_ERROR(ReparseBadRanges(
        bad_joins, input, normalized, norm_to_orig, arena, *model_,
        [this](absl::string_view input, absl::string_view normalized,
               absl::Span<const size_t> norm_to_orig,
               const EncodeResult& result, SentencePieceText* spt,
               size_t input_start_offset) {
          return PopulateSentencePieceText(input, normalized, norm_to_orig,
                                           result, spt, /*skip_surface=*/false,
                                           input_start_offset);
        },
        spt_chunks, input_chunk_boundaries));
  }

  if (spt != nullptr) {
    spt->set_text(input.data(), input.size());
  }

  return absl::OkStatus();
}

absl::Status SentencePieceProcessor::ParallelEncode(
    absl::string_view input, int chunk_len, ThreadPool& thread_pool,
    std::vector<std::string>* pieces) const {
  std::vector<int> ids;
  return ParallelEncodeInternal(input, chunk_len, thread_pool, pieces, &ids,
                                nullptr);
}

absl::Status SentencePieceProcessor::ParallelEncode(
    absl::string_view input, int chunk_len, ThreadPool& thread_pool,
    std::vector<int>* ids) const {
  std::vector<std::string> pieces;
  return ParallelEncodeInternal(input, chunk_len, thread_pool, &pieces, ids,
                                nullptr);
}

absl::Status SentencePieceProcessor::ParallelEncode(
    absl::string_view input, int chunk_len, ThreadPool& thread_pool,
    SentencePieceText* spt) const {
  return ParallelEncodeInternal(input, chunk_len, thread_pool, nullptr, nullptr,
                                spt);
}

#define RET_CHECK_OR_RETURN_DEFAULT(value)                                   \
  if (!status().ok()) {                                                      \
    ABSL_LOG(ERROR) << status().message() << "\nReturns default value " << value; \
    return value;                                                            \
  }

absl::Status SentencePieceProcessor::Normalize(absl::string_view input,
                                               std::string* normalized) const {
  RET_CHECK(normalizer_);
  return normalizer_->Normalize(input, normalized, nullptr);
}

absl::Status SentencePieceProcessor::Normalize(
    absl::string_view input, std::string* normalized,
    std::vector<size_t>* norm_to_orig) const {
  RET_CHECK(normalizer_);
  return normalizer_->Normalize(input, normalized, norm_to_orig);
}

std::string SentencePieceProcessor::Normalize(absl::string_view input) const {
  std::string normalized;
  Normalize(input, &normalized).IgnoreError();
  return normalized;
}

int SentencePieceProcessor::GetPieceSize() const {
  RET_CHECK_OR_RETURN_DEFAULT(0);
  return model_->GetPieceSize();
}

int SentencePieceProcessor::PieceToId(absl::string_view piece) const {
  RET_CHECK_OR_RETURN_DEFAULT(0);
  return model_->PieceToId(piece);
}

const std::string& SentencePieceProcessor::IdToPiece(int id) const {
  static const std::string* kEmptyString = new std::string;
  RET_CHECK_OR_RETURN_DEFAULT(*kEmptyString);
  return model_->IdToPiece(id);
}

bool SentencePieceProcessor::SafeIdToPiece(int id, std::string* piece) const {
  RET_CHECK_OR_RETURN_DEFAULT(false);
  if (id < 0 || id >= model_->GetPieceSize()) {
    return false;
  }
  *piece = IdToPiece(id);
  return true;
}

float SentencePieceProcessor::GetScore(int id) const {
  RET_CHECK_OR_RETURN_DEFAULT(0.0);
  return model_->GetScore(id);
}

bool SentencePieceProcessor::IsControl(int id) const {
  RET_CHECK_OR_RETURN_DEFAULT(0);
  return model_->IsControl(id);
}

bool SentencePieceProcessor::IsUnknown(int id) const {
  RET_CHECK_OR_RETURN_DEFAULT(0);
  return model_->IsUnknown(id);
}

bool SentencePieceProcessor::IsUnused(int id) const {
  RET_CHECK_OR_RETURN_DEFAULT(false);
  return model_->IsUnused(id);
}

bool SentencePieceProcessor::IsByte(int id) const {
  RET_CHECK_OR_RETURN_DEFAULT(false);
  return model_->IsByte(id);
}

int SentencePieceProcessor::unk_id() const { return unk_id_; }

int SentencePieceProcessor::bos_id() const { return bos_id_; }

int SentencePieceProcessor::eos_id() const { return eos_id_; }

int SentencePieceProcessor::pad_id() const { return pad_id_; }

template <typename T>
absl::Status SentencePieceProcessor::ApplyExtraOptions(
    absl::Span<const ExtraOption> extra_options, T* output) const {
  for (const auto& extra_option : extra_options) {
    switch (extra_option) {
      case REVERSE:
        if constexpr (std::is_same_v<T, SentencePieceText>) {
          std::reverse(output->mutable_pieces()->begin(),
                       output->mutable_pieces()->end());
        } else {
          std::reverse(output->begin(), output->end());
        }
        break;
      case EOS:
        if (const int id = eos_id(); id != -1) {
          if constexpr (std::is_same_v<T, SentencePieceText>) {
            auto* piece = output->add_pieces();
            piece->set_id(id);
            piece->set_piece(model_->eos_piece().data(),
                             model_->eos_piece().size());
            piece->set_begin(output->text().size());
            piece->set_end(output->text().size());
          } else {
            using V = typename T::value_type;
            if constexpr (std::is_same_v<V, int>) {
              output->emplace_back(id);
            } else {
              output->emplace_back(model_->eos_piece());
            }
          }
        }
        break;
      case BOS:
        if (const int id = bos_id(); id != -1) {
          if constexpr (std::is_same_v<T, SentencePieceText>) {
            auto* array = output->mutable_pieces();
            array->Add();
            for (int i = array->size() - 1; i > 0; --i) {
              array->SwapElements(i - 1, i);
            }
            auto* piece = array->Mutable(0);
            piece->set_id(id);
            piece->set_piece(model_->bos_piece().data(),
                             model_->bos_piece().size());
            piece->set_begin(0);
            piece->set_end(0);
          } else {
            using V = typename T::value_type;
            if constexpr (std::is_same_v<V, int>) {
              output->emplace(output->begin(), id);
            } else {
              output->emplace(output->begin(), model_->bos_piece());
            }
          }
        }
        break;
      case UNK_PIECE:
        if constexpr (std::is_same_v<T, SentencePieceText>) {
          for (int i = 0; i < output->pieces_size(); ++i) {
            auto* piece = output->mutable_pieces(i);
            if (IsUnknown(piece->id())) {
              piece->set_piece(model_->unk_piece().data(),
                               model_->unk_piece().size());
            }
          }
        }
        break;
      default:
        if constexpr (std::is_same_v<T, SentencePieceText>) {
          output->Clear();
        } else {
          output->clear();
        }
        return absl::InternalError("unknown extra_option type.");
    }
  }
  return absl::OkStatus();
}

bool SentencePieceProcessor::HasUnkPieceOption() const {
  for (const auto& option : encode_extra_options_) {
    if (option == UNK_PIECE) return true;
  }
  return false;
}

// static
absl::Status SentencePieceProcessor::ParseExtraOptions(
    absl::string_view _extra_option,
    std::vector<SentencePieceProcessor::ExtraOption>* extra_options) const {
  absl::string_view extra_option(_extra_option.data(), _extra_option.size());

  extra_options->clear();
  if (extra_option.empty()) return absl::OkStatus();

  RETURN_IF_ERROR(status());

  static std::map<absl::string_view, SentencePieceProcessor::ExtraOption>
      extra_option_map = {{"bos", SentencePieceProcessor::BOS},
                          {"eos", SentencePieceProcessor::EOS},
                          {"reverse", SentencePieceProcessor::REVERSE},
                          {"unk", SentencePieceProcessor::UNK_PIECE},
                          {"unk_piece", SentencePieceProcessor::UNK_PIECE}};
  for (const auto& s : absl::StrSplit(extra_option, ':')) {
    const auto it = extra_option_map.find(s);
    RET_CHECK(it != extra_option_map.end())
        << "option \"" << s << "\" is not available.";
    extra_options->push_back(it->second);

    if (it->second == SentencePieceProcessor::BOS) {
      RET_CHECK(bos_id() != -1)
          << "id for `" << model_->bos_piece() << "` is not defined.";
    }
    if (it->second == SentencePieceProcessor::EOS) {
      RET_CHECK(eos_id() != -1)
          << "id for `" << model_->eos_piece() << "` is not defined.";
    }
  }
  return absl::OkStatus();
}

void SentencePieceProcessor::SetModel(std::unique_ptr<ModelInterface>&& model) {
  model_ = std::move(model);
}

void SentencePieceProcessor::SetNormalizer(
    std::unique_ptr<normalizer::Normalizer>&& normalizer) {
  normalizer_ = std::move(normalizer);
}

const ModelProto& SentencePieceProcessor::model_proto() const {
  return *model_proto_;
}

std::string SentencePieceProcessor::serialized_model_proto() const {
  return model_proto_ ? model_proto_->SerializeAsString() : "";
}

NormalizerSpec* SentencePieceProcessor::mutable_normalizer_spec() const {
  return model_proto_ ? model_proto_->mutable_normalizer_spec() : nullptr;
}

// Set seed value of random generator.
// Do not set static_cast<unique_int>(-1),
// as this seed is reserved for initializing from
// std::random_device.
void SetRandomGeneratorSeed(unsigned int seed);

template <typename T>
absl::Status SentencePieceProcessor::EncodeOptimized(
    absl::string_view input, std::vector<T>* output) const {
  RET_CHECK_STATUS_STL(output);

  if (input.empty()) {
    output->clear();
    return ApplyExtraOptions(encode_extra_options_, output);
  }

  std::string normalized;
  RETURN_IF_ERROR(
      normalizer_->Normalize(input, &normalized, /*norm_to_orig=*/nullptr));
  const EncodeResult result = model_->Encode(normalized);
  const bool byte_fallback_enabled = model_->ByteFallbackEnabled();
  const bool has_unk_piece = HasUnkPieceOption();
  bool is_prev_unk = false;
  output->clear();
  output->reserve(result.size());

  for (const auto& piece : result) {
    const absl::string_view w = piece.first;
    RET_CHECK(!piece.first.empty()) << "Empty piece is not allowed.";
    const int id = piece.second;
    if (IsControl(id)) {
      if constexpr (std::is_same_v<T, int>) {
        output->emplace_back(id);
      } else {
        output->emplace_back(w.data(), w.size());
      }
      is_prev_unk = false;
    } else {
      const bool is_unk = IsUnknown(id);
      if (is_unk && byte_fallback_enabled) {
        for (size_t i = 0; i < w.size(); ++i) {
          if constexpr (std::is_same_v<T, int>) {
            const auto sp_id =
                model_->PieceToId(ByteToPiece(static_cast<uint8_t>(w[i])));
            output->emplace_back(sp_id);
          } else {
            output->emplace_back(ByteToPiece(static_cast<uint8_t>(w[i])));
          }
        }
      } else {
        // Merge continuous runs of unknown pieces.
        if (!is_prev_unk || !is_unk) {
          if constexpr (std::is_same_v<T, int>) {
            output->emplace_back(id);
          } else {
            if (is_unk && has_unk_piece) {
              output->emplace_back(model_->unk_piece());
            } else {
              output->emplace_back(w.data(), w.size());
            }
          }
        } else {
          if constexpr (!std::is_same_v<T, int>) {
            if (!has_unk_piece) {
              output->back().append(w.data(), w.size());
            }
          }
        }
      }
      is_prev_unk = is_unk;
    }
  }

  return ApplyExtraOptions(encode_extra_options_, output);
}

template <typename T>
absl::Status SentencePieceProcessor::DecodeOptimized(
    absl::Span<const T> input, std::string* detokenized) const {
  RET_CHECK_STATUS_STL(detokenized);

  if (input.empty()) {
    return absl::OkStatus();
  }

  // active_input points to the data we will decode. By default it points to the
  // input span (zero-copy).
  absl::Span<const T> active_input = input;
  // If we have extra options to apply, we must copy the input to a mutable
  // container (work_input) to apply the modifications, as the input span is
  // const.
  std::vector<T> work_input;

  if (!decode_extra_options_.empty()) {
    work_input.assign(input.begin(), input.end());
    RETURN_IF_ERROR(ApplyExtraOptions(decode_extra_options_, &work_input));
    active_input = work_input;
  }

  absl::string_view unk_surface = kDefaultUnknownSymbol;
  if (model_proto_ && model_proto_->trainer_spec().has_unk_surface())
    unk_surface = model_proto_->trainer_spec().unk_surface();

  std::string byte_queue;

  auto ProcessByteQueue = [&]() -> absl::Status {
    if (byte_queue.empty()) return absl::OkStatus();
    int offset = 0;
    const int bytes_len = byte_queue.size();
    while (offset < bytes_len) {
      size_t consumed;
      const bool is_valid = string_util::IsValidDecodeUTF8(
          absl::string_view(byte_queue).substr(offset), &consumed);
      if (!is_valid) {
        RET_CHECK_EQ(consumed, 1);
        absl::StrAppend(detokenized, kReplacementCharacter);
      } else {
        const absl::string_view utf8 =
            absl::string_view(byte_queue).substr(offset, consumed);
        absl::StrAppend(detokenized, utf8);
      }
      offset += consumed;
    }
    byte_queue.clear();
    return absl::OkStatus();
  };

  bool is_bos_ws = true;
  for (const auto& item : active_input) {
    int id = -1;
    absl::string_view piece;
    if constexpr (std::is_same_v<T, int>) {
      id = item;
      if (id < 0 || id >= GetPieceSize()) {
        return absl::Status(absl::StatusCode::kOutOfRange,
                            absl::StrCat("Invalid id: ", id));
      }
      piece = IdToPiece(id);
    } else {
      piece = item;
      id = PieceToId(piece);
    }

    if (IsByte(id)) {
      const int byte = PieceToByte(piece);
      RET_CHECK_LE(0, byte);
      byte_queue.append(1, byte);
    } else {
      RETURN_IF_ERROR(ProcessByteQueue());
      if (!detokenized->empty()) {
        is_bos_ws = false;
      }

      if (IsControl(id)) {
        continue;
      }

      absl::string_view p = piece;
      bool has_bos_ws = false;
      if (is_bos_ws &&
          (!model_proto_ ||
           (model_proto_ &&
            (model_proto_->normalizer_spec().add_dummy_prefix() ||
             model_proto_->normalizer_spec().remove_extra_whitespaces())))) {
        has_bos_ws = absl::ConsumePrefix(&p, kSpaceSymbol);
        if (model_proto_ &&
            model_proto_->normalizer_spec().remove_extra_whitespaces()) {
          has_bos_ws = false;
        }
      }

      if (IsUnknown(id)) {
        if (IdToPiece(id) == piece) {
          absl::StrAppend(detokenized, unk_surface);
        } else {
          absl::StrAppend(detokenized, piece);
        }
      } else {
        absl::StrAppend(detokenized,
                        absl::StrReplaceAll(p, {{kSpaceSymbol, " "}}));
      }

      if (has_bos_ws || !detokenized->empty()) {
        is_bos_ws = false;
      }
    }
  }
  RETURN_IF_ERROR(ProcessByteQueue());

  if (denormalizer_) {
    *detokenized = denormalizer_->Normalize(*detokenized);
  }

  return absl::OkStatus();
}

namespace io {
absl::Status LoadModelProto(absl::string_view filename,
                            ModelProto* model_proto) {
  if (filename.empty()) {
    return absl::NotFoundError("model file path should not be empty.");
  }

  auto input = filesystem::NewReadableFile(filename, true);
  RETURN_IF_ERROR(input->status());
  std::string serialized;
  if (!input->ReadAll(&serialized)) {
    return absl::InternalError(absl::StrCat("could not read ", filename));
  }
  if (!model_proto->ParseFromArray(serialized.data(), serialized.size())) {
    return absl::InternalError(
        absl::StrCat("could not parse ModelProto from ", filename));
  }

  return absl::OkStatus();
}

absl::Status SaveModelProto(absl::string_view filename,
                            const ModelProto& model_proto) {
  if (filename.empty()) {
    return absl::NotFoundError("model file path should not be empty.");
  }
  auto output = filesystem::NewWritableFile(filename, true);
  RETURN_IF_ERROR(output->status());
  RET_CHECK(output->Write(model_proto.SerializeAsString()));

  return absl::OkStatus();
}
}  // namespace io
}  // namespace sentencepiece
