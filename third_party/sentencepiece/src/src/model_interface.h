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

#ifndef MODEL_INTERFACE_H_
#define MODEL_INTERFACE_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "normalizer.h"
#include "sentencepiece_model.pb.h"
#include "sentencepiece_processor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "third_party/darts_clone/darts.h"
#include "util.h"

namespace sentencepiece {

// "_this_is_a_pen" => ["_this", "_is", "_a", "_pen"]
std::vector<absl::string_view> SplitIntoWords(
    absl::string_view text, bool treat_ws_as_suffix = false,
    bool allow_ws_only_pieces = false);

// Converts byte (0-255) to piece (e.g., 58 -> "<0x3A>").
const std::string& ByteToPiece(unsigned char c);

// Converts piece to byte (e.g., "<0x3A>" -> 58). Returns -1 if `piece` is not
// a valid byte piece.
int PieceToByte(absl::string_view piece);

using EncodeResult = std::vector<std::pair<absl::string_view, int>>;
using NBestEncodeResult = std::vector<std::pair<EncodeResult, float>>;

class ModelProto;

// Underlying model interface.
// Given a normalized string, returns a sequence of sentence pieces with ids.
class ModelInterface {
 public:
  using PieceToIdMap = absl::flat_hash_map<absl::string_view, int>;

  [[nodiscard]] absl::string_view unk_piece() const;
  [[nodiscard]] absl::string_view bos_piece() const;
  [[nodiscard]] absl::string_view eos_piece() const;
  [[nodiscard]] absl::string_view pad_piece() const;

  // `model_proto` should not be deleted until ModelInterface is destroyed.
  explicit ModelInterface(const ModelProto& model_proto);
  ModelInterface() = default;

  virtual ~ModelInterface();

  // Returns Status.
  // Encode/Decode functions are valid only when status is OK.
  virtual absl::Status status() const { return status_; }

  [[nodiscard]] virtual const ModelProto& model_proto() const {
    return *model_proto_;
  }

  [[nodiscard]] virtual const normalizer::PrefixMatcher* prefix_matcher()
      const {
    return matcher_.get();
  }

  // Given a normalized string, returns a sequence of sentence pieces with ids.
  // The concatenation of pieces must be the same as `normalized`.
  [[nodiscard]] virtual EncodeResult Encode(
      absl::string_view normalized) const = 0;

  // The same as above, but returns nbest result with score.
  [[nodiscard]] virtual NBestEncodeResult NBestEncode(
      absl::string_view normalized, int nbest_size) const {
    ABSL_LOG(ERROR) << "Not implemented.";
    return {};
  }

  [[nodiscard]] virtual EncodeResult SampleEncode(absl::string_view normalized,
                                                  float alpha) const {
    ABSL_LOG(ERROR) << "Not implemented.";
    return {};
  }

  // Sample `samples` many tokenisations from the segmentation lattice
  // If `wor` is true, the samples are taken without replacement, and the scores
  // are the inclusion probabilities of the elements in the sample; otherwise
  // the samples are taken with replacement and the scores are the log-probs of
  // sample elements
  // If `include_best` is true, the best tokenisation is always included in the
  // sample, and the remaining elements are sampled excluding the best.
  [[nodiscard]] virtual NBestEncodeResult SampleEncodeAndScore(
      absl::string_view normalized, float alpha, int samples, bool wor,
      bool include_best) const {
    ABSL_LOG(ERROR) << "Not implemented.";
    return {{EncodeResult(), 0.0}};
  }

  // Calculates the entropy of the segmentation lattice with inverse temperature
  // `alpha`. Uses a novel dynamic program to calculate the entropy.
  [[nodiscard]] virtual float CalculateEntropy(absl::string_view normalized,
                                               float alpha) const {
    ABSL_LOG(ERROR) << "Not implemented.";
    return 0.0;
  }

  // Return true if SampleEncode returns a valid result.
  [[nodiscard]] virtual bool IsSampleEncodeAvailable() const { return false; }

  // Return true if NBestEncode returns a valid result.
  [[nodiscard]] virtual bool IsNBestEncodeAvailable() const { return false; }

  // Return true if SampleEncodeAndScore returns a valid result.
  [[nodiscard]] virtual bool IsSampleEncodeAndScoreAvailable() const {
    return false;
  }

  // Return true if CalculateEntropy returns a valid result.
  [[nodiscard]] virtual bool IsCalculateEntropyAvailable() const {
    return false;
  }

  // Returns the vocab id of `piece`.
  // both `pieces_` and `reserved_id_map_` are checked.
  // Returns UNK(0) if `piece` is unknown
  [[nodiscard]] virtual int PieceToId(absl::string_view piece) const;

  // Returns the vocab id of `piece`.
  // Returns UNK(0) if `piece` is unknown
  // It does not use reserved_id_map_ for the optimization sake,
  // e.g., BPE merges only with normal pieces.
  [[nodiscard]] int PieceToIdNoReserved(absl::string_view piece) const;

  // Returns the string representation of vocab with `id`.
  // id must be 0 <= id < GetPieceSize().
  [[nodiscard]] virtual const std::string& IdToPiece(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return model_proto_->pieces(id).piece();
  }

  // Returns the size of sentence pieces, which is the same
  // as the size of vocabulary for NMT.
  [[nodiscard]] virtual int GetPieceSize() const {
    if (model_proto_ == nullptr) {
      return 0;
    }
    return model_proto_->pieces_size();
  }

  // Returns the score of `id`.
  // Score represents a log probability of the piece.
  // We can roughly estimate the unigram frequency of the piece.
  [[nodiscard]] virtual float GetScore(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return model_proto_->pieces(id).score();
  }

  // Returns true if `id` is unknown symbol.
  [[nodiscard]] virtual bool IsUnknown(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::UNKNOWN);
  }

  // Returns true if `id` is control symbol.
  [[nodiscard]] virtual bool IsControl(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::CONTROL);
  }

  // Returns true if `id` is unused symbol.
  [[nodiscard]] virtual bool IsUnused(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::UNUSED);
  }

  // Returns true if `id` is user defined symbol.
  [[nodiscard]] virtual bool IsUserDefined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::USER_DEFINED);
  }

  // Returns true if `id` is byte symbol.
  [[nodiscard]] virtual bool IsByte(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() == ModelProto::SentencePiece::BYTE);
  }

  [[nodiscard]] virtual bool ByteFallbackEnabled() const {
    return (model_proto_ != nullptr) &&
           model_proto_->trainer_spec().byte_fallback();
  }

  // Verifies if the `expected` and `actual` outputs are equivalent. `expected`
  // and `actual` are sentence pieces joined by space (` `). Normally it means
  // that the two strings are identical. In some model, due to float rounding
  // errors, the strings may not be identical, but they may be still equivalent
  // provided their scores are close enough (by some espilon).
  [[nodiscard]] virtual bool VerifyOutputsEquivalent(
      absl::string_view expected, absl::string_view actual) const {
    return expected == actual;
  }

 protected:
  // Initializes pieces_ and reserved_id_map_.
  // Control/special symbols (type CONTROL, UNKNOWN, BYTE) are stored in
  // `reserved_id_map_` instead of `pieces_`. This excludes them from the main
  // vocabulary map, which in turn prevents them from being merged during BPE
  // segmentation (since BPE merge only looks up in `pieces_`).
  void InitializePieces(bool use_reserved_id_map);

  // Non-virtual (inlined) implementation for faster execution.
  [[nodiscard]] float GetScoreInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return model_proto_->pieces(id).score();
  }

  [[nodiscard]] bool IsUnknownInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::UNKNOWN);
  }

  [[nodiscard]] bool IsControlInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::CONTROL);
  }

  [[nodiscard]] bool IsUnusedInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::UNUSED);
  }

  [[nodiscard]] bool IsUserDefinedInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() ==
            ModelProto::SentencePiece::USER_DEFINED);
  }

  [[nodiscard]] bool IsByteInlined(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    return (model_proto_->pieces(id).type() == ModelProto::SentencePiece::BYTE);
  }

  [[nodiscard]] bool IsReservedId(int id) const {
    ABSL_DCHECK_GE(id, 0);
    ABSL_DCHECK_LT(id, model_proto_->pieces_size());
    const auto& piece = model_proto_->pieces(id);
    return (piece.type() != ModelProto::SentencePiece::NORMAL &&
            piece.type() != ModelProto::SentencePiece::USER_DEFINED &&
            piece.type() != ModelProto::SentencePiece::UNUSED);
  }

  const ModelProto* model_proto_ = nullptr;

  // PrefixMatcher for user defined symbols.
  std::unique_ptr<normalizer::PrefixMatcher> matcher_;

  // piece -> id map for normal pieces
  PieceToIdMap pieces_;

  // piece -> id map for control, unknown, and byte pieces
  PieceToIdMap reserved_id_map_;

  // unknown id.
  int unk_id_ = 0;

  // status.
  absl::Status status_;
};
}  // namespace sentencepiece
#endif  // MODEL_INTERFACE_H_
