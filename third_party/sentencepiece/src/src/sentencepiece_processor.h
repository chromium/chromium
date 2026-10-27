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

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
namespace absl {
using std::string_view;
}  // namespace absl

#include "absl/status/status.h"

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
//   sp.Encode("hello world.", &sps).IgnoreError();
//
//   vector<int> ids;
//   sp.Encode("hello world.", &ids).IgnoreError();
//
//   string detok;
//   sp.Decode(sps, &detok);
//   ABSL_CHECK_EQ("hello world.", detok).IgnoreError();
//
//   sp.Decode(ids, &detok);
//   ABSL_CHECK_EQ("hello world.", detok).IgnoreError();
//
//  We can also use SentencePieceText which manages the byte-offsets
//  between user input (output) and internal sentence pieces.
//
//   SentencePieceText spt;
//   sp.Encode("hello world.", &spt);
//   // Emits the byte range of each piece.
//   for (const auto &piece : spt.pieces()) {
//      ABSL_LOG(INFO) << piece.begin() << " " << piece.end();
//   }
//
//   sp.Decode({0, 1, 2, 3..}, &spt);
//   for (const auto &piece : spt.pieces()) {
//      ABSL_LOG(INFO) << piece.begin() << " " << piece.end();
//   }
//

class NBestSentencePieceText;
class ModelInterface;
class SentencePieceText;
class ModelProto;
class NormalizerSpec;

namespace normalizer {
class Normalizer;
}  // namespace normalizer

// Default ThreadPool implemented using Abseil functionality.
// If you want to use a custom implementation, please inherit from it.
//
// Note: This ThreadPool does not support recursive calls. Scheduling a new task
// on the same ThreadPool from within an already scheduled task will cause a
// severe deadlock. Please use a different ThreadPool instance instead.
class ThreadPool {
 public:
  ThreadPool() = delete;
  explicit ThreadPool(size_t num_threads);
  virtual ~ThreadPool();

  virtual void Schedule(std::function<void()> func);
  [[nodiscard]] virtual size_t num_threads() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Currently, the C++ API does not include a dedicated batch processing API.
// However, you can safely perform batch processing in coordination with the
// existing ThreadPool by using the RunBatch utility below.
//
// Executes tasks concurrently with dynamic load-balancing. Stops early if any
// task returns an error.
// `total_tasks`: Number of tasks to execute (= batch size)
// `task_func`:   Function to process a task by index.
// `pool`:        ThreadPool for scheduling workers.
//
// Sample:
//
// ThreadPool pool(32);
// std::vector<std::string> ins = {...};
// std::vector<std::vector<int>> outs(ins.size());
// auto status = sentencepiece::RunBatch(inputs.size(), [&](size_t i) {
//   return spm.Encode(ins[i], &outs[i]);
// }, pool);
absl::Status RunBatch(size_t total_tasks,
                      std::function<absl::Status(size_t index)> task_func,
                      ThreadPool& pool);

namespace util {
using bytes = std::string;
}  // namespace util

class NBestSentencePieceText;
class ModelInterface;
class SentencePieceText;
class SentencePieceText_SentencePiece;

// Wrapper class of SentencePieceText
// This wrapper only allows an immutable access to the proto and
// hides the actual implementation of protobuf.
// See sentencepiece.proto for the details of this class.
class ImmutableSentencePieceText_ImmutableSentencePiece {
 public:
  ImmutableSentencePieceText_ImmutableSentencePiece();
  ~ImmutableSentencePieceText_ImmutableSentencePiece() = default;

  [[nodiscard]] const std::string& piece() const;
  [[nodiscard]] const std::string& surface() const;
  [[nodiscard]] uint32_t id() const;
  [[nodiscard]] uint32_t begin() const;
  [[nodiscard]] uint32_t end() const;

  friend class ImmutableSentencePieceText;

 private:
  explicit ImmutableSentencePieceText_ImmutableSentencePiece(
      const SentencePieceText_SentencePiece& sp);
  const SentencePieceText_SentencePiece* sp_ = nullptr;
};

class ImmutableSentencePieceText {
 public:
  ImmutableSentencePieceText();
  virtual ~ImmutableSentencePieceText();

  [[nodiscard]] std::vector<ImmutableSentencePieceText_ImmutableSentencePiece>
  pieces() const;

  [[nodiscard]] size_t pieces_size() const;
  [[nodiscard]] ImmutableSentencePieceText_ImmutableSentencePiece pieces(
      int index) const;

  [[nodiscard]] const std::string& text() const;
  [[nodiscard]] float score() const;

  [[nodiscard]] util::bytes SerializeAsString() const;

  // Returns the actual mutable proto.
  // Do not use this outside of SentencePieceProcessor, as
  // it returns the raw pointer managed by the shared_ptr.
  SentencePieceText* mutable_proto();

  // Converts the utf8 byte spans into Unicode char span.
  void ConvertToUnicodeSpans();

  friend class ImmutableNBestSentencePieceText;

 private:
  explicit ImmutableSentencePieceText(const SentencePieceText& spt);
  const SentencePieceText* spt_ = nullptr;
  std::shared_ptr<SentencePieceText> rep_;
};

// Wrapper class of SentencePieceText
// This wrapper only allows an immutable access to the proto and
// hides the actual implementation of protobuf.
// See sentencepiece.proto for the details of this class.
class ImmutableNBestSentencePieceText {
 public:
  ImmutableNBestSentencePieceText();
  virtual ~ImmutableNBestSentencePieceText();

  [[nodiscard]] std::vector<ImmutableSentencePieceText> nbests() const;

  [[nodiscard]] size_t nbests_size() const;
  [[nodiscard]] ImmutableSentencePieceText nbests(int index) const;

  [[nodiscard]] util::bytes SerializeAsString() const;

  // Returns the actual mutable proto.
  // Do not use this outside of SentencePieceProcessor, as
  // it returns the raw pointer managed by the shared_ptr.
  NBestSentencePieceText* mutable_proto();

  void ConvertToUnicodeSpans();

 private:
  std::shared_ptr<NBestSentencePieceText> rep_;
};

class SentencePieceProcessor {
 public:
  SentencePieceProcessor();
  virtual ~SentencePieceProcessor();

  // Loads model from `filename`.
  // Returns false if `filename` cannot be loaded.
  virtual absl::Status Load(absl::string_view filename);

  // Loads model from `filename`.
  // Crash if `filename` cannot be loaded.
  virtual void LoadOrDie(absl::string_view filename);

  // Loads model from `model_proto`.
  // `model_proto` is copied.
  virtual absl::Status Load(const ModelProto& model_proto);

  // Loads model from `model_proto`.
  // `model_proto` is moved.
  virtual absl::Status Load(std::unique_ptr<ModelProto> model_proto);

  // Loads model from `serialized`, which is a string-serialized model proto.
  // Useful to load the model from a platform independent blob object.
  virtual absl::Status LoadFromSerializedProto(absl::string_view serialized);

  // Returns the status. Encode/Decode methods are valid when status is OK.
  virtual absl::Status status() const;

  // Sets encode extra_option sequence.
  virtual absl::Status SetEncodeExtraOptions(absl::string_view extra_option);

  // Sets decode extra_option sequence.
  virtual absl::Status SetDecodeExtraOptions(absl::string_view extra_option);

  //////////////////////////////////////////////////////////////
  // Vocabulary restriction.
  // Background:
  // https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt

  // Restricts the vocabulary set.
  // The input sentences are encoded into the tokens in `valid_vocab`.
  virtual absl::Status SetVocabulary(
      const std::vector<absl::string_view>& valid_vocab);

  // Reverts the vocabulary restriction.
  virtual absl::Status ResetVocabulary();

  // Loads the valid vocabulary set from `filename` in TSV format.
  // Format:  <token> <tab> <freq>.
  // Any token with frequency < threshold will be treated as OOV.
  virtual absl::Status LoadVocabulary(absl::string_view filename,
                                      int threshold);

  //////////////////////////////////////////////////////////////
  // Simple Encode and Decode API.
  //
  // Given a UTF8 input, encodes it into a sequence of sentence pieces.
  virtual absl::Status Encode(absl::string_view input,
                              std::vector<std::string>* pieces) const;

  // Given a UTF8 input, encodes it into a sequence of ids.
  virtual absl::Status Encode(absl::string_view input,
                              std::vector<int>* ids) const;

  // Given a sequence of pieces, decodes it into a detokenized output.
  virtual absl::Status Decode(absl::Span<const std::string> pieces,
                              std::string* detokenized) const;

  // Given a sequence of pieces, decodes it into a detokenized output.
  virtual absl::Status Decode(absl::Span<const absl::string_view> pieces,
                              std::string* detokenized) const;

  // Given a sequence of ids, decodes it into a detokenized output.
  virtual absl::Status Decode(absl::Span<const int> ids,
                              std::string* detokenized) const;

  // Backward compatibility overloads for std::vector.
  virtual absl::Status Decode(const std::vector<std::string>& pieces,
                              std::string* detokenized) const {
    return Decode(absl::MakeConstSpan(pieces), detokenized);
  }

  virtual absl::Status Decode(const std::vector<absl::string_view>& pieces,
                              std::string* detokenized) const {
    return Decode(absl::MakeConstSpan(pieces), detokenized);
  }

  virtual absl::Status Decode(const std::vector<int>& ids,
                              std::string* detokenized) const {
    return Decode(absl::MakeConstSpan(ids), detokenized);
  }

  //////////////////////////////////////////////////////////////
  // NBest API.
  //
  // Same as Encode, but returns nbest results.
  virtual absl::Status NBestEncode(
      absl::string_view input, int nbest_size,
      std::vector<std::vector<std::string>>* pieces) const;

  // Same as Encode, but returns nbest results.
  virtual absl::Status NBestEncode(absl::string_view input, int nbest_size,
                                   std::vector<std::vector<int>>* ids) const;

  //////////////////////////////////////////////////////////////
  // Sampling API.
  //
  // Unigram and BPE support sampling mode.
  // - Unigram (--model_type=unigram):
  // `nbest_size`: When `nbest_size` is positive value, approximately samples
  // one segmentation from nbest candidates. When `nbest_size` is negative
  // value, samples one segmentation from the hypotheses (Lattice) according to
  // the generation probabilities using forward-filtering and backward-sampling
  // algorithm.
  // `alpha`: Smoothing parameter (inverse temperature). The best segmentation
  // (Viterbi segmentation) is more likely sampled when setting larger alpha.
  // When alpha is 0.0, one segmentation is uniformly sampled from the nbest or
  // lattice. `nbest_size` and `alpha` correspond to parameters `l` and `alpha`
  // in https://arxiv.org/abs/1804.10959  (nbest_size < 0 means l = infinity)
  //
  // - BPE (--model_type=bpe):
  // `alpha`: The dropout probability `p` of bpe merge operations in
  // https://arxiv.org/abs/1910.13267 Nbest-based sampling is not supported so
  // nbest_size parameter is ignored in BPE.
  virtual absl::Status SampleEncode(absl::string_view input, int nbest_size,
                                    float alpha,
                                    std::vector<std::string>* pieces) const;

  // Same as above, but returns a sequence of ids.
  virtual absl::Status SampleEncode(absl::string_view input, int nbest_size,
                                    float alpha, std::vector<int>* ids) const;

  //////////////////////////////////////////////////////////////
  // SampleEncodeAndScore API.
  //
  // Sample `samples` many tokenisations from the segmentation lattice.
  // These methods are only available in model_type=unigram.
  //
  // `alpha`: smoothing parameter (inverse temperature). The same as `alpha` in
  // `Sample` method.
  // 'wor`: If `wor` is true, the samples are taken without replacement, and the
  // scores are the inclusion probabilities of the elements in the sample;
  // otherwise the samples are taken with replacement and the scores are the
  // log-probs of sample elements
  // `include_best`: If `include_best` is true, the best tokenisation is always
  // included in the sample, and the remaining elements are sampled excluding
  // the best.
  virtual absl::Status SampleEncodeAndScore(
      absl::string_view input, int num_samples, float alpha, bool wor,
      bool include_best,
      std::vector<std::pair<std::vector<std::string>, float>>* pieces) const;

  // Same as above, but returns a sequence of ids.
  virtual absl::Status SampleEncodeAndScore(
      absl::string_view input, int num_samples, float alpha, bool wor,
      bool include_best,
      std::vector<std::pair<std::vector<int>, float>>* ids) const;

  //////////////////////////////////////////////////////////////
  // Entropy API.
  //
  // This only available in model_type=unigram.
  // Calculate entropy of possible tokenisations
  virtual absl::Status CalculateEntropy(absl::string_view input, float alpha,
                                        float* entropy) const;

  //////////////////////////////////////////////////////////////
  // Advanced API returning SentencePieceText, which manages
  // utf8-byte alignments between user-input/detokenized text
  // and internal sentencepiece sequence.
  //
  // Given a UTF8 input, encodes it into SentencePieceText.
  //
  // When using these APIs, sentencepiece.pb.h header files must be included.
  // We can also use ImmutableSentencePieceText as follows.
  //
  // ImmutableSentencePieceText spt;
  // Encode("hello", spt.mutable_proto()).IgnoreError();
  // std::cout << spt.pieces_size() << std::endl;
  virtual absl::Status Encode(absl::string_view input,
                              SentencePieceText* spt) const;

  virtual absl::Status NBestEncode(absl::string_view input, int nbest_size,
                                   NBestSentencePieceText* nbest_spt) const;

  virtual absl::Status SampleEncode(absl::string_view input, int nbest_size,
                                    float alpha, SentencePieceText* spt) const;

  virtual absl::Status SampleEncodeAndScore(
      absl::string_view input, int num_samples, float alpha, bool wor,
      bool include_best, NBestSentencePieceText* samples_spt) const;

  // DEPRECATED: Remove this API and use absl::Span<const absl::string_view>
  virtual absl::Status Decode(absl::Span<const std::string> pieces,
                              SentencePieceText* spt) const;

  virtual absl::Status Decode(absl::Span<const absl::string_view> pieces,
                              SentencePieceText* spt) const;

  virtual absl::Status Decode(absl::Span<const int> ids,
                              SentencePieceText* spt) const;

  // Backward compatibility overloads for std::vector.
  virtual absl::Status Decode(const std::vector<std::string>& pieces,
                              SentencePieceText* spt) const {
    return Decode(absl::MakeConstSpan(pieces), spt);
  }

  virtual absl::Status Decode(const std::vector<absl::string_view>& pieces,
                              SentencePieceText* spt) const {
    return Decode(absl::MakeConstSpan(pieces), spt);
  }

  virtual absl::Status Decode(const std::vector<int>& ids,
                              SentencePieceText* spt) const {
    return Decode(absl::MakeConstSpan(ids), spt);
  }

  //////////////////////////////////////////////////////////////
  // API methods for encoding sequences in parallel.
  // This is particularly useful for long inputs.

  // chunk_len controls how long each chunk to be tokenized in parallel is.
  // For best results, set this to ~10000.

  // WARNING: ParallelEncode with SentencePieceText * inputs currently does not
  // copy the UNK surface form correctly. Use at your own risk!
  virtual absl::Status ParallelEncode(absl::string_view input, int chunk_len,
                                      ThreadPool& thread_pool,
                                      std::vector<std::string>* pieces) const;
  virtual absl::Status ParallelEncode(absl::string_view input, int chunk_len,
                                      ThreadPool& thread_pool,
                                      std::vector<int>* ids) const;
  virtual absl::Status ParallelEncode(absl::string_view input, int chunk_len,
                                      ThreadPool& thread_pool,
                                      SentencePieceText* spt) const;

#define DEFINE_SPP_DIRECT_FUNC_IMPL(FuncName, OutType, ...) \
  OutType output;                                           \
  const auto status = FuncName(__VA_ARGS__, &output);       \
  return output;

#define DEFINE_SPP_SERIALIZED_PROTO_IMPL(FuncName, OutType, ...)     \
  OutType output;                                                    \
  const auto status = FuncName(__VA_ARGS__, output.mutable_proto()); \
  return output.SerializeAsString();

#define DEFINE_SPP_IMMUTABLE_PROTO_IMPL(FuncName, OutType, ...)      \
  OutType output;                                                    \
  const auto status = FuncName(__VA_ARGS__, output.mutable_proto()); \
  return output;

  //////////////////////////////////////////////////////////////
  // Handy methods that return the result directly.
  // These functions ignore internal errors.
  [[nodiscard]] virtual std::vector<std::string> EncodeAsPieces(
      absl::string_view input) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Encode, std::vector<std::string>, input);
  }

  [[nodiscard]] virtual std::vector<int> EncodeAsIds(
      absl::string_view input) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Encode, std::vector<int>, input);
  }

  [[nodiscard]] virtual std::vector<std::vector<std::string>>
  NBestEncodeAsPieces(absl::string_view input, int nbest_size) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(
        NBestEncode, std::vector<std::vector<std::string>>, input, nbest_size);
  }

  [[nodiscard]] virtual std::vector<std::vector<int>> NBestEncodeAsIds(
      absl::string_view input, int nbest_size) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(NBestEncode, std::vector<std::vector<int>>,
                                input, nbest_size);
  }

  [[nodiscard]] virtual std::vector<std::string> SampleEncodeAsPieces(
      absl::string_view input, int nbest_size, float alpha) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncode, std::vector<std::string>, input,
                                nbest_size, alpha);
  }

  [[nodiscard]] virtual std::vector<int> SampleEncodeAsIds(
      absl::string_view input, int nbest_size, float alpha) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncode, std::vector<int>, input,
                                nbest_size, alpha);
  }

  [[nodiscard]] virtual std::vector<std::pair<std::vector<std::string>, float>>
  SampleEncodeAndScoreAsPieces(absl::string_view input, int num_samples,
                               float alpha, bool wor, bool include_best) const {
    using _T = std::vector<std::pair<std::vector<std::string>, float>>;
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncodeAndScore, _T, input, num_samples,
                                alpha, wor, include_best);
  }

  [[nodiscard]] virtual std::vector<std::pair<std::vector<int>, float>>
  SampleEncodeAndScoreAsIds(absl::string_view input, int num_samples,
                            float alpha, bool wor, bool include_best) const {
    using _T = std::vector<std::pair<std::vector<int>, float>>;
    DEFINE_SPP_DIRECT_FUNC_IMPL(SampleEncodeAndScore, _T, input, num_samples,
                                alpha, wor, include_best);
  }

  virtual std::vector<std::string> ParallelEncodeAsPieces(
      absl::string_view input, int chunk_len, ThreadPool& therad_pool) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(ParallelEncode, std::vector<std::string>, input,
                                chunk_len, therad_pool);
  }

  virtual std::vector<int> ParallelEncodeAsIds(absl::string_view input,
                                               int chunk_len,
                                               ThreadPool& therad_pool) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(ParallelEncode, std::vector<int>, input,
                                chunk_len, therad_pool);
  }

  // DEPRECATED: Remove this API and use std::vector<std::string_view>
  [[nodiscard]] virtual std::string DecodePieces(
      const std::vector<std::string>& pieces) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Decode, std::string, pieces);
  }

  [[nodiscard]] virtual std::string DecodePieces(
      const std::vector<absl::string_view>& pieces) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Decode, std::string, pieces);
  }

  [[nodiscard]] virtual std::string DecodeIds(
      const std::vector<int>& ids) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(Decode, std::string, ids);
  }

  [[nodiscard]] virtual float CalculateEntropy(absl::string_view text,
                                               float alpha) const {
    DEFINE_SPP_DIRECT_FUNC_IMPL(CalculateEntropy, float, text, alpha);
  }

  //////////////////////////////////////////////////////////////
  // SerializedProto API. (DEPRECATED). Use ImmutableProto API.
  // They are used in Python interface. Returns serialized proto.
  // In python module, we can get access to the full Proto after
  // deserialzing the returned byte sequence.
  [[nodiscard]] virtual util::bytes EncodeAsSerializedProto(
      absl::string_view input) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(Encode, ImmutableSentencePieceText, input);
  }

  [[nodiscard]] virtual util::bytes SampleEncodeAsSerializedProto(
      absl::string_view input, int nbest_size, float alpha) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(SampleEncode, ImmutableSentencePieceText,
                                     input, nbest_size, alpha);
  }

  [[nodiscard]] virtual util::bytes NBestEncodeAsSerializedProto(
      absl::string_view input, int nbest_size) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(
        NBestEncode, ImmutableNBestSentencePieceText, input, nbest_size);
  }

  [[nodiscard]] virtual util::bytes SampleEncodeAndScoreAsSerializedProto(
      absl::string_view input, int num_samples, float alpha, bool wor,
      bool include_best) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(SampleEncodeAndScore,
                                     ImmutableNBestSentencePieceText, input,
                                     num_samples, alpha, wor, include_best);
  }

  virtual util::bytes ParallelEncodeAsSerializedProto(
      absl::string_view input, int chunk_len, ThreadPool& thread_pool) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(ParallelEncode, ImmutableSentencePieceText,
                                     input, chunk_len, thread_pool);
  }

  // TODO(taku): Remove this API and use std::vector<std::string_view>
  [[nodiscard]] virtual util::bytes DecodePiecesAsSerializedProto(
      const std::vector<std::string>& pieces) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(Decode, ImmutableSentencePieceText,
                                     pieces);
  }

  [[nodiscard]] virtual util::bytes DecodePiecesAsSerializedProto(
      const std::vector<absl::string_view>& pieces) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(Decode, ImmutableSentencePieceText,
                                     pieces);
  }

  [[nodiscard]] virtual util::bytes DecodeIdsAsSerializedProto(
      const std::vector<int>& ids) const {
    DEFINE_SPP_SERIALIZED_PROTO_IMPL(Decode, ImmutableSentencePieceText, ids);
  }

  //////////////////////////////////////////////////////////////
  // ImmutableProto API.
  [[nodiscard]] virtual ImmutableSentencePieceText EncodeAsImmutableProto(
      absl::string_view input) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(Encode, ImmutableSentencePieceText, input);
  }

  [[nodiscard]] virtual ImmutableSentencePieceText SampleEncodeAsImmutableProto(
      absl::string_view input, int nbest_size, float alpha) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(SampleEncode, ImmutableSentencePieceText,
                                    input, nbest_size, alpha);
  }

  [[nodiscard]] virtual ImmutableNBestSentencePieceText NBestEncodeAsImmutableProto(
      absl::string_view input, int nbest_size) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(
        NBestEncode, ImmutableNBestSentencePieceText, input, nbest_size);
  }

  [[nodiscard]] virtual ImmutableNBestSentencePieceText SampleEncodeAndScoreAsImmutableProto(
      absl::string_view input, int num_samples, float alpha, bool wor,
      bool include_best) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(SampleEncodeAndScore,
                                    ImmutableNBestSentencePieceText, input,
                                    num_samples, alpha, wor, include_best);
  }

  virtual ImmutableSentencePieceText ParallelEncodeAsImmutableProto(
      absl::string_view input, int chunk_len, ThreadPool& thread_pool) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(ParallelEncode, ImmutableSentencePieceText,
                                    input, chunk_len, thread_pool);
  }

  // TODO(taku): Remove this API and use std::vector<std::string_view>
  [[nodiscard]] virtual ImmutableSentencePieceText DecodePiecesAsImmutableProto(
      const std::vector<std::string>& pieces) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(Decode, ImmutableSentencePieceText, pieces);
  }

  [[nodiscard]] virtual ImmutableSentencePieceText DecodePiecesAsImmutableProto(
      const std::vector<absl::string_view>& pieces) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(Decode, ImmutableSentencePieceText, pieces);
  }

  [[nodiscard]] virtual ImmutableSentencePieceText DecodeIdsAsImmutableProto(
      const std::vector<int>& ids) const {
    DEFINE_SPP_IMMUTABLE_PROTO_IMPL(Decode, ImmutableSentencePieceText, ids);
  }

#undef DEFINE_SPP_DIRECT_FUNC_IMPL
#undef DEFINE_SPP_SERIALIZED_PROTO_IMPL
#undef DEFINE_SPP_IMMUTABLE_PROTO_IMPL

  //////////////////////////////////////////////////////////////
  // Normalization methods.

  // Normalize `input`.
  virtual absl::Status Normalize(absl::string_view input,
                                 std::string* normalized) const;

  // Normalize `input`. Stores the utf8-byte offset from
  // the normalized string to the original input.
  virtual absl::Status Normalize(absl::string_view input,
                                 std::string* normalized,
                                 std::vector<size_t>* norm_to_orig) const;

  [[nodiscard]] virtual std::string Normalize(absl::string_view input) const;

  //////////////////////////////////////////////////////////////
  // Vocabulary management methods.
  //
  // Returns the size of sentence pieces, which is the same as
  // the size of vocabulary for NMT.
  [[nodiscard]] virtual int GetPieceSize() const;

  // Returns the vocab id of `piece`.
  // Returns UNK(0) if `piece` is unknown.
  [[nodiscard]] virtual int PieceToId(absl::string_view piece) const;

  // Returns the string representation of vocab with `id`.
  [[nodiscard]] virtual const std::string& IdToPiece(int id) const;

  // Returns the string representation of vocab with `id`.
  // Returns false when id is out of range.
  virtual bool SafeIdToPiece(int id, std::string* piece) const;

  // Returns the score of `id`.
  // Usually score is an emission log probability of unigram language
  // model.
  [[nodiscard]] virtual float GetScore(int id) const;

  // Returns true if `id` is unknown symbol.
  [[nodiscard]] virtual bool IsUnknown(int id) const;

  // Returns true if `id` is control symbol.
  [[nodiscard]] virtual bool IsControl(int id) const;

  // Returns true if `id` is unused symbol.
  [[nodiscard]] virtual bool IsUnused(int id) const;

  // Returns true if `id` is byte symbol.
  [[nodiscard]] virtual bool IsByte(int id) const;

  // Returns the reserved id.
  // Returns -1 if not defined.
  //
  // Note: Valid IDs are returned only when they are strictly defined as
  // CONTROL tokens (or UNKNOWN for unk_id). If they are defined as
  // USER_DEFINED, these methods will return -1, as USER_DEFINED symbols
  // are treated as normal symbols (protected from segmentation) rather
  // than strict special control symbols.
  //
  // Consequently, encoding extra options (like "bos" / "eos") and Python
  // wrapper flags (like add_bos=True / add_eos=True) will be IGNORED if
  // the corresponding tokens are not strictly defined as CONTROL tokens.

  // Returns unknown (<unk>) id.
  [[nodiscard]] virtual int unk_id() const;

  // Returns BOS (<s>) id.
  [[nodiscard]] virtual int bos_id() const;

  // Returns EOS (</s>) id.
  [[nodiscard]] virtual int eos_id() const;

  // Returns PAD (<pad>) id.
  [[nodiscard]] virtual int pad_id() const;

  //////////////////////////////////////////////////////////////
  // Model management.
  //
  // Allows injection of a mock model instance. `model` is moved.
  void SetModel(std::unique_ptr<ModelInterface>&& model);

  // Allows injection of a normalizer instance. `normalizer` is moved.
  void SetNormalizer(std::unique_ptr<normalizer::Normalizer>&& normalizer);

  // Returns immutable model proto. Useful to obtain extended
  // or experimental parameters encoded in model_proto.
  [[nodiscard]] const ModelProto& model_proto() const;

  // returns immutable model proto as std::string.
  // Useful to save the state of this instance via Python's pickle object.
  [[nodiscard]] util::bytes serialized_model_proto() const;

  // Returns mutable normalizer_spec.
  // Updating the intenral normalization during the encoding/decoding are not
  // recommended and may result in unexpected behavior. Use at your own risk.
  [[nodiscard]] NormalizerSpec* mutable_normalizer_spec() const;

 private:
  enum ExtraOption { REVERSE, BOS, EOS, UNK_PIECE };

  absl::Status ParseExtraOptions(absl::string_view extra_option,
                                 std::vector<ExtraOption>* extra_options) const;

  template <typename T>
  absl::Status ApplyExtraOptions(absl::Span<const ExtraOption> extra_options,
                                 T* output) const;

  template <typename T>
  absl::Status EncodeOptimized(absl::string_view input,
                               std::vector<T>* output) const;

  template <typename T>
  absl::Status DecodeOptimized(absl::Span<const T> input,
                               std::string* detokenized) const;

  bool HasUnkPieceOption() const;

  absl::Status PopulateSentencePieceText(
      absl::string_view input, absl::string_view normalized,
      absl::Span<const size_t> norm_to_orig,
      const std::vector<std::pair<absl::string_view, int>>& result,
      SentencePieceText* spt, bool skip_surface = false,
      size_t input_start_offset = 0) const;

  absl::Status ParallelEncodeInternal(absl::string_view input, size_t chunk_len,
                                      ThreadPool& thread_pool,
                                      std::vector<std::string>* pieces,
                                      std::vector<int>* ids,
                                      SentencePieceText* spt) const;

  std::unique_ptr<ModelInterface> model_;
  std::unique_ptr<normalizer::Normalizer> normalizer_;
  std::unique_ptr<normalizer::Normalizer> denormalizer_;

  // Cached IDs.
  // Note that these IDs are not always the same as the IDs in TrainerSpec.
  // The TrainerSpec defines the training-time configuration, while these
  // IDs reflect the actual IDs in the loaded model, which might be different
  // or disabled (set to -1).
  int unk_id_ = -1;
  int bos_id_ = -1;
  int eos_id_ = -1;
  int pad_id_ = -1;

  // Underlying model protocol buffer. The same lifetime as model_.
  std::unique_ptr<ModelProto> model_proto_;

  std::vector<ExtraOption> encode_extra_options_;
  std::vector<ExtraOption> decode_extra_options_;
};

// Set seed value of random generator.
// Do not set static_cast<unique_int>(-1),
// as this seed is reserved for initializing from
// std::random_device.
void SetRandomGeneratorSeed(unsigned int seed);

// Set the global log level. The default loglevel is 0.
// The log is emitted only when min_log_level >= output_log_level.
void SetMinLogLevel(int v);

// Sets global timeout in milliseconds for NBestEncode.
// If timeout is reached, the search falls back to Viterbi.
// The default value is 30000 (30 seconds).
// 0 or negative value means no timeout.
void SetNBestTimeout(int timeout_ms);

// IO related functions to absorb model formats.
namespace io {
// Loads `model_proto` from `filename`.
// We can instantiate SentencePieceProcessor as follows:
//
//  auto model_proto = absl::make_unique<ModelProto>();
//  io::LoadModelProto("//path/spm.model", model_proto.get());
//  SentencePieceProcessor sp;
//  ABSL_CHECK_OK(sp.Load(std::move(model_proto)));
absl::Status LoadModelProto(absl::string_view, ModelProto* model_proto);

// Saves `model_proto` as `filename`.
absl::Status SaveModelProto(absl::string_view, const ModelProto& model_proto);
}  // namespace io
}  // namespace sentencepiece
#endif  // SENTENCEPIECE_PROCESSOR_H_
