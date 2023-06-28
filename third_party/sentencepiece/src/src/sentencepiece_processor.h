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

#ifndef SENTENCEPIECE_PROCESSOR_H_
#define SENTENCEPIECE_PROCESSOR_H_

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "src/common.h"


namespace sentencepiece {

// SentencePieceProcessor:
// Simple and language independent tokenizer and de-tokenizer for
// Neural Network Machine Translation.
//
// SentencePieceProcessor provides Encode() and Decode() methods,
// which correspond to tokenization and de-tokenization respectively.
//
// - Encode:
//   Given a raw source sentence, encode it into a sequence
//   of pieces or vocabulary ids.
//
// - Decode:
//   Given a sequence of pieces or vocabulary ids, decode it
//   into a de-tokenized raw sentence.
//
// SentencePieceProcessor provides a lossless data conversion
// that allows the original raw sentence to be perfectly reconstructed
// from the encoded data, i.e., Decode(Encode(input)) == input.
// This characteristics is useful, as we can make the de-tokenization
// completely language independent.
//
// Usage:
//   SentencePieceProcessor sp;
//   sp.Load("//path/to/model");
//
//   vector<string> sps;
//   sp.Encode("hello world.", &sps);
//
//   vector<int> ids;
//   sp.Encode("hello world.", &ids);
//
//   string detok;
//   sp.Decode(sps, &detok);
//   CHECK_EQ("hello world.", detok);
//
//   sp.Decode(ids, &detok);
//   CHECK_EQ("hello world.", detok);
//
//  We can also use SentencePieceText which manages the byte-offsets
//  between user input (output) and internal sentence pieces.
//
//   SentencePieceText spt;
//   sp.Encode("hello world.", &spt);
//   // Emits the byte range of each piece.
//   for (const auto &piece : spt.pieces()) {
//      LOG(INFO) << piece.begin() << " " << piece.end();
//   }
//
//   sp.Decode({0, 1, 2, 3..}, &spt);
//   for (const auto &piece : spt.pieces()) {
//      LOG(INFO) << piece.begin() << " " << piece.end();
//   }
//

class NBestSentencePieceText;
class ModelInterface;
class SentencePieceText;
class ModelProto;

namespace normalizer {
class Normalizer;
}  // namespace normalizer

class SentencePieceProcessor {
 public:
  SentencePieceProcessor();
  virtual ~SentencePieceProcessor();

  // Loads model from `filename`.
  // Returns false if `filename` cannot be loaded.
  virtual ::util::Status Load(absl::string_view filename);

  // Loads model from `filename`.
  // Crash if `filename` cannot be loaded.
  virtual void LoadOrDie(absl::string_view filename);

  // Loads model from `model_proto`.
  // `model_proto` is copied.
  virtual ::util::Status Load(const ModelProto &model_proto);

  // Loads model from `model_proto`.
  // `model_proto` is moved.
  virtual ::util::Status Load(std::unique_ptr<ModelProto> model_proto);

  // Loads model from `serialized`, which is a string-serialized model proto.
  // Useful to load the model from a platform independent blob object.
  virtual ::util::Status LoadFromSerializedProto(absl::string_view serialized);

  // Returns the status. Encode/Decode methods are valid when status is OK.
  virtual ::util::Status status() const;

  // Sets encode extra_option sequence.
  virtual ::util::Status SetEncodeExtraOptions(absl::string_view extra_option);

  // Sets decode extra_option sequence.
  virtual ::util::Status SetDecodeExtraOptions(absl::string_view extra_option);

  //////////////////////////////////////////////////////////////
  // Vocabulary restriction.
  // Background:
  // https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt

  // Restricts the vocabulary set.
  // The input sentences are encoded into the tokens in `valid_vocab`.
  virtual ::util::Status SetVocabulary(
      const std::vector<std::string> &valid_vocab);

  // Reverts the vocabulary restriction.
  virtual ::util::Status ResetVocabulary();

  // Loads the valid vocabulary set from `filename` in TSV format.
  // Format:  <token> <tab> <freq>.
  // Any token with frequency < threshold will be treated as OOV.
  virtual ::util::Status LoadVocabulary(absl::string_view filename,
                                        int threshold);

  //////////////////////////////////////////////////////////////
  // Simple API.
  //
  // Given a UTF8 input, encodes it into a sequence of sentence pieces.
  virtual ::util::Status Encode(absl::string_view input,
                                std::vector<std::string> *pieces) const;

  // Given a UTF8 input, encodes it into a sequence of ids.
  virtual ::util::Status Encode(absl::string_view input,
                                std::vector<int> *ids) const;

  // Given a sequence of pieces, decodes it into a detokenized output.
  virtual ::util::Status Decode(const std::vector<std::string> &pieces,
                                std::string *detokenized) const;

  // Given a sequence of ids, decodes it into a detokenized output.
  virtual ::util::Status Decode(const std::vector<int> &ids,
                                std::string *detokenized) const;

  //////////////////////////////////////////////////////////////
  // NBest API.
  // Same as Encode, but returns nbest results.
  virtual ::util::Status NBestEncode(
      absl::string_view input, int nbest_size,
      std::vector<std::vector<std::string>> *pieces) const;

  // Same as Encode, but returns nbest results.
  virtual ::util::Status NBestEncode(absl::string_view input, int nbest_size,
                                     std::vector<std::vector<int>> *ids) const;

  //////////////////////////////////////////////////////////////
  // Sampling API
  // When `nbest_size` is positive value, approximately samples one
  // segmentation from nbest candidates. When `nbest_size` is negative value,
  // samples one segmentation from the hypotheses (Lattice) according to the
  // generation probabilities using forward-filtering and backward-sampling
  // algorithm. `alpha` is a smoothing parameter.  The best segmentation
  // (Viterbi segmentation) is more likely sampled when setting larger
  // alpha. When alpha is 0.0, one segmentation is uniformly sampled from the
  // nbest or lattice.
  // `nbest_size` and `alpha` correspond to parameters `l` and `alpha`
  // in https://arxiv.org/abs/1804.10959  (nbest_size < 0 means l = infinity)
  virtual ::util::Status SampleEncode(absl::string_view input, int nbest_size,
                                      float alpha,
                                      std::vector<std::string> *pieces) const;

  // Same as above, but returns a sequence of ids.
  virtual ::util::Status SampleEncode(absl::string_view input, int nbest_size,
                                      float alpha, std::vector<int> *ids) const;

  //////////////////////////////////////////////////////////////
  // Advanced API returning SentencePieceText, which manages
  // utf8-byte alignments between user-input/detokenized text
  // and internal sentencepiece sequence.
  //
  // Given a UTF8 input, encodes it into SentencePieceText.
  virtual ::util::Status Encode(absl::string_view input,
                                SentencePieceText *spt) const;

  // Same as above, but returns NBestSentencePieceText.
  virtual ::util::Status NBestEncode(absl::string_view input, int nbest_size,
                                     NBestSentencePieceText *nbest_spt) const;

  // Same as above, but samples one segmentation from the hypotheses
  // (Lattice).
  virtual ::util::Status SampleEncode(absl::string_view input, int nbest_size,
                                      float alpha,
                                      SentencePieceText *spt) const;

  // Given a sequence of pieces, decodes it into SentencePieceText.
  virtual ::util::Status Decode(const std::vector<std::string> &pieces,
                                SentencePieceText *spt) const;

  // Given a sequence of ids, decodes it into SentencePieceText.
  virtual ::util::Status Decode(const std::vector<int> &ids,
                                SentencePieceText *spt) const;

  //////////////////////////////////////////////////////////////
  // Handy methods that return the result directly.
  // These functions ignore internal errors.
#ifdef SWIG
#define DEFINE_SPP_DIRECT_FUNC_IMPL(FuncName, OutType, ...) \
  OutType output;                                           \
  const auto _status = FuncName(__VA_ARGS__, &output);      \
  if (!_status.ok()) throw _status;                         \
  return output;
#else
#define DEFINE_SPP_DIRECT_FUNC_IMPL(FuncName, OutType, ...) \
  OutType output;                                           \
  FuncName(__VA_ARGS__, &output).IgnoreError();             \
  return output;
#endif

  virtual std::vector<std::string> EncodeAsPieces(
      absl::string_view input) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Encode, std::vector<std::string>, input);
  }

  virtual std::vector<int> EncodeAsIds(absl::string_view input) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Encode, std::vector<int>, input);
  }

  virtual std::vector<std::vector<std::string>> NBestEncodeAsPieces(
      absl::string_view input, int nbest_size) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(
        NBestEncode, std::vector<std::vector<std::string>>, input, nbest_size);
  }

  virtual std::vector<std::vector<int>> NBestEncodeAsIds(
      absl::string_view input, int nbest_size) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(NBestEncode, std::vector<std::vector<int>>,
                                input, nbest_size);
  }

  virtual std::vector<std::string> SampleEncodeAsPieces(absl::string_view input,
                                                        int nbest_size,
                                                        float alpha) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncode, std::vector<std::string>, input,
                                nbest_size, alpha);
  }

  virtual std::vector<int> SampleEncodeAsIds(absl::string_view input,
                                             int nbest_size,
                                             float alpha) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncode, std::vector<int>, input,
                                nbest_size, alpha);
  }

  virtual std::string DecodePieces(
      const std::vector<std::string> &pieces) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Decode, std::string, pieces);
  }

  virtual std::string DecodeIds(const std::vector<int> &ids) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Decode, std::string, ids);
  }

#undef DEFINE_SPP_DIRECT_FUNC_IMPL

  // They are used in Python interface. Returns serialized proto.
  // In python module, we can get access to the full Proto after
  // deserialzing the returned byte sequence.
  virtual std::string EncodeAsSerializedProto(absl::string_view input) const;

  virtual std::string SampleEncodeAsSerializedProto(absl::string_view input,
                                                    int nbest_size,
                                                    float alpha) const;

  virtual std::string NBestEncodeAsSerializedProto(absl::string_view input,
                                                   int nbest_size) const;

  virtual std::string DecodePiecesAsSerializedProto(
      const std::vector<std::string> &pieces) const;

  virtual std::string DecodeIdsAsSerializedProto(
      const std::vector<int> &ids) const;

  //////////////////////////////////////////////////////////////
  // Vocabulary management methods.
  //
  // Returns the size of sentence pieces, which is the same as
  // the size of vocabulary for NMT.
  virtual int GetPieceSize() const;

  // Returns the vocab id of `piece`.
  // Returns UNK(0) if `piece` is unknown.
  virtual int PieceToId(absl::string_view piece) const;

  // Returns the string representation of vocab with `id`.
  virtual const std::string &IdToPiece(int id) const;

  // Returns the score of `id`.
  // Usually score is an emission log probability of unigram language model.
  virtual float GetScore(int id) const;

  // Returns true if `id` is unknown symbol.
  virtual bool IsUnknown(int id) const;

  // Returns true if `id` is control symbol.
  virtual bool IsControl(int id) const;

  // Returns true if `id` is unused symbol.
  virtual bool IsUnused(int id) const;

  // Returns the reserved id.
  // Returns -1 if not defined.

  // Returns unknown (<unk>) id.
  virtual int unk_id() const;

  // Returns BOS (<s>) id.
  virtual int bos_id() const;

  // Returns EOS (</s>) id.
  virtual int eos_id() const;

  // Returns PAD (<pad>) id.
  virtual int pad_id() const;

#ifndef SWIG
  //////////////////////////////////////////////////////////////
  // Model management.
  //
  // Allows injection of a mock model instance. `model` is moved.
  void SetModel(std::unique_ptr<ModelInterface> &&model);

  // Allows injection of a normalizer instance. `normalizer` is moved.
  void SetNormalizer(std::unique_ptr<normalizer::Normalizer> &&normalizer);
#endif

  // Returns immutable model proto. Useful to obtain extended
  // or experimental parameters encoded in model_proto.
  const ModelProto &model_proto() const;

 private:
  enum ExtraOption { REVERSE, BOS, EOS };

  ::util::Status ParseExtraOptions(
      absl::string_view extra_option,
      std::vector<ExtraOption> *extra_options) const;

  ::util::Status ApplyExtraOptions(
      const std::vector<ExtraOption> &extra_options,
      SentencePieceText *spt) const;

  ::util::Status PopulateSentencePieceText(
      absl::string_view input, absl::string_view normalized,
      const std::vector<size_t> &norm_to_orig,
      const std::vector<std::pair<absl::string_view, int>> &result,
      SentencePieceText *spt) const;

  std::unique_ptr<ModelInterface> model_;
  std::unique_ptr<normalizer::Normalizer> normalizer_;

  // Underlying model protocol buffer. The same lifetime as model_.
  std::unique_ptr<ModelProto> model_proto_;

  std::vector<ExtraOption> encode_extra_options_;
  std::vector<ExtraOption> decode_extra_options_;
};

#ifndef SWIG
// IO related functions to absorb model formats.
namespace io {
// Loads `model_proto` from `filename`.
// We can instantiate SentencePieceProcessor as follows:
//
//  auto model_proto = std::make_unique<ModelProto>();
//  io::LoadModelProto("//path/spm.model", model_proto.get());
//  SentencePieceProcessor sp;
//  CHECK_OK(sp.Load(std::move(model_proto)));
::util::Status LoadModelProto(absl::string_view, ModelProto *model_proto);

// Saves `model_proto` as `filename`.
::util::Status SaveModelProto(absl::string_view, const ModelProto &model_proto);
}  // namespace io
#endif  // SWIG
}  // namespace sentencepiece
#endif  // SENTENCEPIECE_PROCESSOR_H_
