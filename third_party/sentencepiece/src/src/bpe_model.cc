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

#include <functional>
#include <memory>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "freelist.h"
#include "util.h"

namespace sentencepiece {
namespace bpe {

Model::Model(const ModelProto &model_proto) {
  model_proto_ = &model_proto;
  InitializePieces();
}

Model::~Model() {}

std::vector<std::pair<absl::string_view, int>> Model::SampleEncode(
    absl::string_view normalized,
    float alpha) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  struct SymbolPair {
    int left;     // left index of this pair
    int right;    // right index of this pair
    float score;  // score of this pair. large is better.
    size_t size;  // length of this piece
  };

  class SymbolPairComparator {
   public:
    const bool operator()(SymbolPair *h1, SymbolPair *h2) {
      return (h1->score < h2->score ||
              (h1->score == h2->score && h1->left > h2->left));
    }
  };

  struct Symbol {
    int prev;     // prev index of this symbol. -1 for BOS.
    int next;     // next index of tihs symbol. -1 for EOS.
    bool freeze;  // this symbol is never be merged.
    absl::string_view piece;
  };

  using Agenda = std::priority_queue<SymbolPair *, std::vector<SymbolPair *>,
                                     SymbolPairComparator>;
  Agenda agenda;
  std::vector<Symbol> symbols;
  symbols.reserve(normalized.size());

  // Reverse merge rules.
  // key: merged symbol, value: pair of original symbols.
  absl::flat_hash_map<absl::string_view,
                      std::pair<absl::string_view, absl::string_view>>
      rev_merge;

  // Pre-allocates SymbolPair for efficiency.
  constexpr size_t kPreallocateSymbolPairSize = 256;
  model::FreeList<SymbolPair> symbol_pair_allocator(kPreallocateSymbolPairSize);

  // Lookup new symbol pair at [left, right] and inserts it to agenda.
  auto MaybeAddNewSymbolPair = [this, &symbol_pair_allocator, &symbols, &agenda,
                                &rev_merge](int left, int right) {
    if (left == -1 || right == -1 || symbols[left].freeze ||
        symbols[right].freeze)
      return;
    const absl::string_view piece(
        symbols[left].piece.data(),
        symbols[left].piece.size() + symbols[right].piece.size());
    const auto it = pieces_.find(piece);
    if (it == pieces_.end()) {
      return;
    }
    auto *h = symbol_pair_allocator.Allocate();
    h->left = left;
    h->right = right;
    h->score = GetScore(it->second);
    h->size = piece.size();
    agenda.push(h);

    // Makes `rev_merge` for resegmentation.
    if (IsUnusedInlined(it->second)) {
      rev_merge[piece] =
          std::make_pair(symbols[left].piece, symbols[right].piece);
    }
  };

  // Splits the input into character sequence
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

  // Lookup all bigrams.
  for (size_t i = 1; i < symbols.size(); ++i) {
    MaybeAddNewSymbolPair(i - 1, i);
  }

  // BPE-dropout: https://arxiv.org/pdf/1910.13267.pdf
  std::mt19937* rand_gen = nullptr;
  auto skip_merge = [&]() {
    if (alpha <= 0.0) {
      return false;
    }
    if (alpha >= 1.0) {
      return true;
    }
    if (rand_gen == nullptr) {
      rand_gen = random::GetRandomGenerator();
    }
    std::uniform_real_distribution<> gen(0.0, 1.0);
    return gen(*rand_gen) < alpha;
  };

  // Main loop.
  while (!agenda.empty()) {
    SymbolPair *top = agenda.top();
    agenda.pop();

    // `top` is no longer available.
    if (symbols[top->left].piece.empty() || symbols[top->right].piece.empty() ||
        symbols[top->left].piece.size() + symbols[top->right].piece.size() !=
            top->size) {
      continue;
    }

    // Note that orignal BPE-dropout paper assumes that all merged symbols are
    // pre computed, but here we randomly skip merge opration inside this loop.
    // This implemenation is theoretically equivalent to the original one.
    if (skip_merge()) {
      continue;
    }

    // Replaces symbols with `top` rule.
    symbols[top->left].piece = absl::string_view(
        symbols[top->left].piece.data(),
        symbols[top->left].piece.size() + symbols[top->right].piece.size());

    // Updates prev/next pointers.
    symbols[top->left].next = symbols[top->right].next;
    if (symbols[top->right].next >= 0) {
      symbols[symbols[top->right].next].prev = top->left;
    }
    symbols[top->right].piece = absl::string_view("");

    // Adds new symbol pairs which are newly added after symbol replacement.
    MaybeAddNewSymbolPair(symbols[top->left].prev, top->left);
    MaybeAddNewSymbolPair(top->left, symbols[top->left].next);
  }

  std::function<void(absl::string_view, EncodeResult *)> resegment;
  resegment = [this, &resegment, &rev_merge](absl::string_view w,
                                             EncodeResult *output) -> void {
    const int id = PieceToId(w);
    if (id == -1 || !IsUnusedInlined(id)) {
      output->emplace_back(w, id);
      return;
    }
    const auto p = rev_merge.find(w);
    if (p == rev_merge.end()) {
      // This block will never be called, as `rev_merge` stores all the
      // resegmentation info for unused id.
      output->emplace_back(w, id);
      return;
    }
    // Recursively resegment left and right symbols.
    resegment(p->second.first, output);
    resegment(p->second.second, output);
  };

  EncodeResult output;
  for (int index = 0; index != -1; index = symbols[index].next) {
    CHECK_GE(index, 0);
    CHECK_LT(index, static_cast<int>(symbols.size()));
    resegment(symbols[index].piece, &output);
  }

  return output;
}
}  // namespace bpe
}  // namespace sentencepiece
