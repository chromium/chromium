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

#include "bpe_model.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include "freelist.h"
#include "model_interface.h"
#include "sentencepiece_model.pb.h"
#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "util.h"

namespace sentencepiece {
namespace bpe {
namespace {

// Limit recursion depth to prevent stack overflow on malicious models
// with extremely deep BPE merge chains.
// Must be at namespace scope (not local scope) to avoid MSVC lambda capture
// bugs (C3493) in SampleEncode.
constexpr int kMaxBpeResegmentDepth = 100;

struct SymbolPair {
  union {
    float score;  // score of this pair. large is better.
    int32_t int_score;
  };
  uint32_t left;      // left index of this pair
  int right;          // right index of this pair
  unsigned int size;  // length of this piece
};

class SymbolPairComparator {
 public:
  ABSL_ATTRIBUTE_ALWAYS_INLINE inline bool operator()(const SymbolPair& h1,
                                                      const SymbolPair& h2) {
    const int32_t i1 = h1.int_score;
    const int32_t i2 = h2.int_score;

    // Fast path for the common case where both scores are negative because
    // they are log-probabilities.
    // Note: we use the fact that IEEE 754 floating point format enables
    // to compare the integer representation of negative floats which is
    // cheaper than using float comparison. And it works the same way for
    // little endian and big endian machines because the IEEE 754 format is
    // aligned with the endianness.
    // `(i1 & i2) < 0` is an efficient way to check `i1 < 0 && i2 < 0`.
    if ((i1 & i2) < 0) {
      // For negative floats, their integer representation order is the
      // reverse of the float order. That is, for two negative floats f1, f2,
      // f1 < f2 iff i1 > i2.
      return (i1 > i2) || (i1 == i2 && h1.left > h2.left);
    }

    // Slow path for uncommon cases (mixed signs or both positive).
    // Note: the comparison between NaN and +0 and +1 can be different than
    // if we used float numbers but it should not influence the result.
    bool score_less;
    // If signs are different ((i1 ^ i2) < 0), the negative score is smaller.
    if ((i1 ^ i2) < 0) {
      score_less = i1 < 0;
    } else {
      // If signs are the same (and not both negative), they must both be
      // non-negative. For non-negative floats, integer order is the same as
      // float order.
      score_less = i1 < i2;
    }

    return score_less || (i1 == i2 && h1.left > h2.left);
  }
};

struct Symbol {
  int prev;     // prev index of this symbol. -1 for BOS.
  int next;     // next index of tihs symbol. -1 for EOS.
  bool freeze;  // this symbol is never be merged.
  absl::string_view piece;
};
}  // namespace

Model::Model(const ModelProto& model_proto) {
  model_proto_ = &model_proto;
  // BPE model prevents control symbols from being merged by placing them in
  // reserved_id_map_ (which BPE merge ignores).
  // We use PieceToIdNoReserved() during inference to bypass reserved_id_map_
  // for performance.
  InitializePieces(/* use_reserved_id_map= */ false);
}

Model::~Model() {}

std::vector<std::pair<absl::string_view, int>> Model::SampleEncode(
    absl::string_view normalized, float alpha) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  std::vector<Symbol> symbols;
  symbols.reserve(normalized.size());

  // Splits the input into Symbols doing longest prefix match of the input
  // from pieces(type:UNUSED) in the vocabulary.
  // Does character splitting as a fallback of longest prefix match.
  int index = 0;
  while (!normalized.empty()) {
    Symbol s;
    const int mblen = matcher_->PrefixMatch(normalized, &s.freeze);
    s.piece = absl::string_view(normalized.data(), mblen);
    s.prev = index == 0 ? -1 : index - 1;
    normalized.remove_prefix(mblen);
    s.next = normalized.empty() ? -1 : index + 1;
    ++index;
    symbols.emplace_back(s);
  }

  if (symbols.empty()) {
    return {};
  }

  std::vector<SymbolPair> agenda_vec;
  agenda_vec.reserve(symbols.size());

  // Reverse merge rules.
  // key: merged symbol, value: pair of original symbols.
  absl::flat_hash_map<absl::string_view,
                      std::pair<absl::string_view, absl::string_view>>
      rev_merge;

  // Lookup all bigrams.
  if (symbols.size() > 1) {
    int left = 0;
    int right = 1;
    Symbol* symbol_left = &symbols[left];
    Symbol* symbol_right = &symbols[right];
    for (; right < symbols.size();
         left = right, symbol_left = symbol_right, ++right, ++symbol_right) {
      if (symbol_left->freeze || symbol_right->freeze) continue;
      const absl::string_view piece(
          symbol_left->piece.data(),
          symbol_left->piece.size() + symbol_right->piece.size());
      // Use PieceToIdNoReserved() instead of PieceToId() to bypass
      // reserved_id_map_ lookup. This is both an optimization and a design
      // constraint to prevent CONTROL symbols (which are in reserved_id_map_)
      // from being merged.
      const int id = PieceToIdNoReserved(piece);
      // PieceToIdNoReserved() returns unk_id_ on lookup failure (not found in
      // pieces_). Comparing directly with unk_id_ is a fast way to check if the
      // piece is not a mergeable normal piece, avoiding memory access overhead
      // of IsUnknown().
      if (id == unk_id_ || IsReservedId(id)) continue;
      SymbolPair& h = agenda_vec.emplace_back();
      h.left = left;
      h.right = right;
      h.score = GetScoreInlined(id);
      h.size = piece.size();

      // Makes `rev_merge` for resegmentation.
      if (IsUnusedInlined(id))
        rev_merge[piece] =
            std::make_pair(symbol_left->piece, symbol_right->piece);
    }
  }

  // If alpha is >= 1.0, all merges are dropped so we can skip the entire
  // pair agenda creation and merging process.
  if (alpha < 1.0) {
    using Agenda = std::priority_queue<SymbolPair, std::vector<SymbolPair>,
                                       SymbolPairComparator>;
    Agenda agenda(SymbolPairComparator(), std::move(agenda_vec));
    // Lookup new symbol pair at [left, right] and inserts it to agenda.
    auto MaybeAddNewSymbolPair = [this, &symbols, &agenda, &rev_merge](
                                     int left, int right) {
      if (left == -1 || right == -1) return;
      const Symbol& left_symbol = symbols[left];
      const Symbol& right_symbol = symbols[right];
      if (left_symbol.freeze || right_symbol.freeze) return;
      const absl::string_view piece(
          left_symbol.piece.data(),
          left_symbol.piece.size() + right_symbol.piece.size());
      // Use PieceToIdNoReserved() instead of PieceToId() to bypass
      // reserved_id_map_ lookup. This is both an optimization and a design
      // constraint to prevent CONTROL symbols (which are in reserved_id_map_)
      // from being merged.
      const int id = PieceToIdNoReserved(piece);
      // PieceToIdNoReserved() returns unk_id_ on lookup failure.
      // See explanation above for why we compare directly with unk_id_.
      if (id == unk_id_ || IsReservedId(id)) {
        return;
      }
      SymbolPair h;
      h.left = left;
      h.right = right;
      h.score = GetScoreInlined(id);
      h.size = piece.size();
      agenda.push(h);

      // Makes `rev_merge` for resegmentation.
      if (IsUnusedInlined(id))
        rev_merge[piece] =
            std::make_pair(left_symbol.piece, right_symbol.piece);
    };

    const bool use_dropout = alpha > 0.0;
    absl::BitGen* rand_gen =
        use_dropout ? random::GetRandomGenerator() : nullptr;

    // Main loop.
    while (!agenda.empty()) {
      // Pop the top pair if it is stale.
      const SymbolPair& top_ref = agenda.top();
      if (symbols[top_ref.left].piece.empty() ||
          symbols[top_ref.right].piece.empty() ||
          (symbols[top_ref.left].piece.size() +
               symbols[top_ref.right].piece.size() !=
           top_ref.size)) {
        agenda.pop();
        continue;
      }

      SymbolPair top = agenda.top();
      agenda.pop();

      // Note that original BPE-dropout paper assumes that all merged symbols
      // are pre computed, but here we randomly skip merge operation inside this
      // loop. This implementation is theoretically equivalent to the original
      // one. BPE-dropout: https://arxiv.org/pdf/1910.13267.pdf
      if (use_dropout && absl::Bernoulli(*rand_gen, alpha)) continue;

      Symbol& left_symbol = symbols[top.left];
      Symbol& right_symbol = symbols[top.right];

      // Replaces symbols with `top` rule.
      left_symbol.piece = absl::string_view(
          left_symbol.piece.data(),
          left_symbol.piece.size() + right_symbol.piece.size());

      // Updates prev/next pointers.
      left_symbol.next = right_symbol.next;
      if (right_symbol.next >= 0) {
        symbols[right_symbol.next].prev = top.left;
      }
      right_symbol.piece = absl::string_view("");

      // Adds new symbol pairs which are newly added after symbol replacement.
      MaybeAddNewSymbolPair(left_symbol.prev, top.left);
      MaybeAddNewSymbolPair(top.left, left_symbol.next);
    }
  }

  // Limit recursion depth to prevent stack overflow on malicious models
  // with extremely deep BPE merge chains.

  auto resegment = [this, &rev_merge](auto& self, absl::string_view w,
                                      EncodeResult* output, int depth) -> void {
    const int id = PieceToIdNoReserved(w);
    if (depth > kMaxBpeResegmentDepth) {
      // Gracefully stop recursion and output the merged piece as-is to avoid
      // stack overflow.
      output->emplace_back(w, id);
      return;
    }
    if (id == -1 || !IsUnusedInlined(id)) {
      output->emplace_back(w, id);
      return;
    }
    const auto p = rev_merge.find(w);
    if (p == rev_merge.end()) {
      output->emplace_back(w, id);
      return;
    }
    // Direct recursive calls
    self(self, p->second.first, output, depth + 1);
    self(self, p->second.second, output, depth + 1);
  };

  EncodeResult output;
  output.reserve(symbols.size());
  for (int index = 0; index != -1; index = symbols[index].next) {
    if (index >= 0 && index < static_cast<int>(symbols.size())) {
      resegment(resegment, symbols[index].piece, &output, 0);
    }
  }

  return output;
}
}  // namespace bpe
}  // namespace sentencepiece
