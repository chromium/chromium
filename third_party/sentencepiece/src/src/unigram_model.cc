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

#include "src/unigram_model.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "src/util.h"

namespace sentencepiece {
namespace unigram {
namespace {

// Size of nodes pre-allocated in Lattice.
constexpr size_t kPreallocateLatticeNodeSize = 1024;

// Returns log(exp(x) + exp(y)).
// if init_mode is true, returns log(exp(y)) == y.
// log(\sum_i exp(a[i])) can be computed as
// for (int i = 0; i < a.size(); ++i)
//   x = LogSumExp(x, a[i], i == 0);
inline float LogSumExp(float x, float y, bool init_mode) {
  if (init_mode) {
    return y;
  }
  const float vmin = std::min(x, y);
  const float vmax = std::max(x, y);
  constexpr float kMinusLogEpsilon = 50;
  if (vmax > vmin + kMinusLogEpsilon) {
    return vmax;
  } else {
    return vmax + log(std::exp(static_cast<double>(vmin - vmax)) + 1.0);
  }
}
}  // namespace

Lattice::Lattice() : node_allocator_(kPreallocateLatticeNodeSize) {}
Lattice::~Lattice() {}

const std::vector<Lattice::Node *> &Lattice::begin_nodes(int pos) const {
  return begin_nodes_[pos];
}

const std::vector<Lattice::Node *> &Lattice::end_nodes(int pos) const {
  return end_nodes_[pos];
}

int Lattice::size() const {
  // -1 because surface_ may include the EOS.
  return std::max<int>(0, surface_.size() - 1);
}

int Lattice::utf8_size() const { return sentence_.size(); }

const char *Lattice::sentence() const { return sentence_.data(); }

const char *Lattice::surface(int pos) const { return surface_[pos]; }

Lattice::Node *Lattice::bos_node() const { return end_nodes_[0][0]; }

Lattice::Node *Lattice::eos_node() const { return begin_nodes_[size()][0]; }

Lattice::Node *Lattice::NewNode() {
  Node *node = node_allocator_.Allocate();
  node->node_id = node_allocator_.size() - 1;
  return node;
}

void Lattice::Clear() {
  begin_nodes_.clear();
  end_nodes_.clear();
  sentence_ = absl::string_view("");
  surface_.clear();
  node_allocator_.Free();
}

void Lattice::SetSentence(absl::string_view sentence) {
  Clear();

  sentence_ = sentence;
  surface_.reserve(sentence.size() + 1);

  while (!sentence.empty()) {
    const int mblen = std::min<int>(string_util::OneCharLen(sentence.data()),
                                    sentence.size());
    surface_.push_back(sentence.data());
    sentence.remove_prefix(mblen);
  }
  surface_.push_back(sentence.data());

  const int len = size();
  begin_nodes_.resize(len + 1);
  end_nodes_.resize(len + 1);

  constexpr size_t kReservedNodeSize = 16;
  for (int i = 0; i <= len; ++i) {
    begin_nodes_[i].reserve(kReservedNodeSize);
    end_nodes_[i].reserve(kReservedNodeSize);
  }

  Node *bos = NewNode();
  bos->id = -1;
  bos->pos = 0;
  end_nodes_[0].push_back(bos);

  Node *eos = NewNode();
  eos->id = -1;
  eos->pos = len;
  begin_nodes_[len].push_back(eos);
}

Lattice::Node *Lattice::Insert(int pos, int length) {
  Node *node = NewNode();
  node->pos = pos;
  node->length = length;
  const int utf8_length =
      static_cast<int>(surface(pos + length) - surface(pos));
  node->piece = absl::string_view(surface(pos), utf8_length);
  begin_nodes_[pos].push_back(node);
  end_nodes_[pos + node->length].push_back(node);

  return node;
}

std::vector<Lattice::Node *> Lattice::Viterbi() {
  const int len = size();

  for (int pos = 0; pos <= len; ++pos) {
    for (Node *rnode : begin_nodes_[pos]) {
      rnode->prev = nullptr;
      float best_score = 0.0;
      Node *best_node = nullptr;
      for (Node *lnode : end_nodes_[pos]) {
        const float score = lnode->backtrace_score + rnode->score;
        if (best_node == nullptr || score > best_score) {
          best_node = lnode;
          best_score = score;
        }
      }
      if (best_node == nullptr) {
        LOG(ERROR) << "Failed to find the best path in Viterbi.";
        return {};
      }
      rnode->prev = best_node;
      rnode->backtrace_score = best_score;
    }
  }

  // backtrace
  std::vector<Node *> results;
  for (Node *node = begin_nodes_[len][0]->prev; node->prev != nullptr;
       node = node->prev) {
    results.push_back(node);
  }

  std::reverse(results.begin(), results.end());

  return results;
}

float Lattice::PopulateMarginal(float freq,
                                std::vector<float> *expected) const {
  if (expected == nullptr) return 0.0;

  const int len = size();

  // alpha and beta (accumulative log prob) in Forward Backward.
  // the index of alpha/beta is Node::node_id.
  std::vector<float> alpha(node_allocator_.size(), 0.0);
  std::vector<float> beta(node_allocator_.size(), 0.0);

  for (int pos = 0; pos <= len; ++pos) {
    for (Node *rnode : begin_nodes_[pos]) {
      for (Node *lnode : end_nodes_[pos]) {
        alpha[rnode->node_id] = LogSumExp(alpha[rnode->node_id],
                                          lnode->score + alpha[lnode->node_id],
                                          lnode == end_nodes_[pos][0]);
      }
    }
  }

  for (int pos = len; pos >= 0; --pos) {
    for (Node *lnode : end_nodes_[pos]) {
      for (Node *rnode : begin_nodes_[pos]) {
        beta[lnode->node_id] =
            LogSumExp(beta[lnode->node_id], rnode->score + beta[rnode->node_id],
                      rnode == begin_nodes_[pos][0]);
      }
    }
  }

  const float Z = alpha[begin_nodes_[len][0]->node_id];
  for (int pos = 0; pos < len; ++pos) {
    for (Node *node : begin_nodes_[pos]) {
      if (node->id >= 0) {
        // the index of |expected| is a Node::id, which is a vocabulary id.
        (*expected)[node->id] +=
            freq *
            std::exp(static_cast<double>(alpha[node->node_id] + node->score +
                                         beta[node->node_id] - Z));
      }
    }
  }

  return freq * Z;
}

std::vector<std::vector<Lattice::Node *>> Lattice::NBest(size_t nbest_size) {
  if (nbest_size < 1) {
    LOG(WARNING) << "nbest_size >= 1. Returns empty result.";
    return {};
  }

  if (nbest_size == 1) {
    return {Viterbi()};
  }

  // Uses A* search to enumerate N-bests.
  // Given a lattice, enumerates hypotheses (paths) from EOS.
  // At each partial path x, compute f(x) as follows
  //   f(x) = g(x) + h(x).
  // g(x): the sum of scores from  EOS to the left-most node in x.
  // h(x): a heuristic that estimates the largest score from x to BOS.
  // f(x): the priority to pop a new hypothesis from the priority queue.
  //
  // As left-to-right Viterbi search can tell the *exact* value of h(x),
  // we can obtain the exact n-best results with A*.
  struct Hypothesis {
    Node *node;
    Hypothesis *next;
    float fx;
    float gx;
  };

  class HypothesisComparator {
   public:
    const bool operator()(Hypothesis *h1, Hypothesis *h2) {
      return (h1->fx < h2->fx);
    }
  };

  using Agenda = std::priority_queue<Hypothesis *, std::vector<Hypothesis *>,
                                     HypothesisComparator>;
  constexpr size_t kPreallocatedHypothesisSize = 512;
  model::FreeList<Hypothesis> hypothesis_allocator(kPreallocatedHypothesisSize);

  Agenda agenda;
  std::vector<std::vector<Node *>> results;

  auto *eos = hypothesis_allocator.Allocate();
  eos->node = eos_node();
  eos->next = nullptr;
  eos->fx = eos->node->score;
  eos->gx = eos->node->score;
  agenda.push(eos);

  // Run Viterbi first to fill backtrace score.
  Viterbi();

  while (!agenda.empty()) {
    auto *top = agenda.top();
    agenda.pop();
    auto *node = top->node;

    // Reaches to BOS
    if (node == bos_node()) {
      results.resize(results.size() + 1);
      for (auto *n = top->next; n->next != nullptr; n = n->next) {
        results.back().push_back(n->node);
      }
      if (results.size() == nbest_size) {
        break;
      }
      continue;
    }

    // Expands new node ending at node->pos
    for (Node *lnode : end_nodes(node->pos)) {
      auto *hyp = hypothesis_allocator.Allocate();
      hyp->node = lnode;
      hyp->gx = lnode->score + top->gx;  // just adds node->score
      hyp->fx =
          lnode->backtrace_score + top->gx;  // backtrace_score is h(node).
      hyp->next = top;
      agenda.push(hyp);
    }

    // When the input is too long or contains duplicated phrases,
    // `agenda` will get extremely big. Here we avoid this case by
    // dynamically shrinking the agenda.
    constexpr int kMaxAgendaSize = 100000;
    constexpr int kMinAgendaSize = 512;
    if (agenda.size() >= kMaxAgendaSize) {
      LOG(WARNING) << "Too big agenda. shrinking";
      // Keeps the top `kMinAgendaSize` hypothesis.
      Agenda new_agenda;
      const int size = std::min<int>(kMinAgendaSize, nbest_size * 10);
      for (int i = 0; i < size; ++i) {
        new_agenda.push(agenda.top());
        agenda.pop();
      }
      agenda = std::move(new_agenda);
    }
  }

  return results;
}

std::vector<Lattice::Node *> Lattice::Sample(float theta) {
  const int len = size();
  if (len == 0) return {};

  std::vector<float> alpha(node_allocator_.size(), 0.0);

  for (int pos = 0; pos <= len; ++pos) {
    for (Node *rnode : begin_nodes_[pos]) {
      for (Node *lnode : end_nodes_[pos]) {
        alpha[rnode->node_id] = LogSumExp(
            alpha[rnode->node_id], theta * lnode->score + alpha[lnode->node_id],
            lnode == end_nodes_[pos][0]);
      }
    }
  }

  auto *mt = random::GetRandomGenerator();

  std::vector<Node *> results;
  std::vector<float> probs;

  float Z = alpha[eos_node()->node_id];
  Node *node = eos_node();
  while (true) {
    probs.clear();
    for (const Node *lnode : end_nodes_[node->pos]) {
      probs.push_back(std::exp(static_cast<double>(alpha[lnode->node_id] +
                                                   theta * lnode->score - Z)));
    }
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    node = end_nodes_[node->pos][dist(*mt)];
    if (node == bos_node()) break;

    Z = alpha[node->node_id];
    results.push_back(node);
  }

  std::reverse(results.begin(), results.end());
  return results;
}

// Model::Model() {}
// Model::~Model() {}

void Model::PopulateNodes(Lattice *lattice) const {
  auto get_chars_length = [&lattice](int begin_pos, const char *end) {
    int pos = begin_pos;
    while (lattice->surface(pos) < end) ++pos;
    return pos - begin_pos;
  };

  constexpr float kUnkPenalty = 10.0;
  const float unk_score = min_score() - kUnkPenalty;

  const int len = lattice->size();
  const char *end = lattice->sentence() + lattice->utf8_size();

  // +1 just in case.
  std::vector<Darts::DoubleArray::result_pair_type> trie_results(
      trie_results_size_ + 1);

  for (int begin_pos = 0; begin_pos < len; ++begin_pos) {
    const char *begin = lattice->surface(begin_pos);

    // Finds all pieces which are prefix of surface(begin_pos).
    const size_t num_nodes = trie_->commonPrefixSearch(
        begin, trie_results.data(), trie_results.size(),
        static_cast<int>(end - begin));
    CHECK_LT(num_nodes, trie_results.size());

    bool has_single_node = false;

    // Inserts pieces to the lattice.
    for (size_t k = 0; k < num_nodes; ++k) {
      const int length =
          get_chars_length(begin_pos, begin + trie_results[k].length);
      const int id = trie_results[k].value;
      if (IsUnusedInlined(id)) continue;
      Lattice::Node *node = lattice->Insert(begin_pos, length);
      node->id = id;  // the value of Trie stores vocab_id.
      // User defined symbol receives extra bonus to always be selected.
      node->score = IsUserDefinedInlined(id) ? (length * max_score_ + 1.0)
                                             : GetScoreInlined(id);
      if (!has_single_node && node->length == 1) {
        has_single_node = true;
      }
    }

    if (!has_single_node) {
      Lattice::Node *node = lattice->Insert(begin_pos, 1);
      node->id = unk_id_;  // add UNK node.
      node->score = unk_score;
    }
  }
}

int Model::PieceToId(absl::string_view piece) const {
  auto it = reserved_id_map_.find(piece);
  if (it != reserved_id_map_.end()) {
    return it->second;
  }
  int id = 0;
  trie_->exactMatchSearch(piece.data(), id);
  return id == -1 ? unk_id_ : id;
}

void Model::BuildTrie(std::vector<std::pair<absl::string_view, int>> *pieces) {
  if (!status().ok()) return;

  if (pieces->empty()) {
    status_ = ::util::InternalError("no pieces are loaded.");
    return;
  }

  // sort by sentencepiece since DoubleArray::build()
  // only accepts sorted strings.
  sort(pieces->begin(), pieces->end());

  // Makes key/value set for DoubleArrayTrie.
  std::vector<const char *> key(pieces->size());
  std::vector<int> value(pieces->size());
  for (size_t i = 0; i < pieces->size(); ++i) {
    key[i] = (*pieces)[i].first.data();  // sorted piece.
    value[i] = (*pieces)[i].second;      // vocab_id
  }

  trie_ = absl::make_unique<Darts::DoubleArray>();
  if (trie_->build(key.size(), const_cast<char **>(&key[0]), nullptr,
                   &value[0]) != 0) {
    status_ = ::util::InternalError("cannot build double-array.");
    return;
  }

  // Computes the maximum number of shared prefixes in the trie.
  const int kMaxTrieResultsSize = 1024;
  std::vector<Darts::DoubleArray::result_pair_type> results(
      kMaxTrieResultsSize);
  trie_results_size_ = 0;
  for (const auto &p : *pieces) {
    const int num_nodes = trie_->commonPrefixSearch(
        p.first.data(), results.data(), results.size(), p.first.size());
    trie_results_size_ = std::max(trie_results_size_, num_nodes);
  }

  pieces_.clear();

  if (trie_results_size_ == 0)
    status_ = ::util::InternalError("no entry is found in the trie.");
}

Model::Model(const ModelProto &model_proto) {
  model_proto_ = &model_proto;

  InitializePieces();

  min_score_ = FLT_MAX;
  max_score_ = FLT_MIN;
  for (const auto &sp : model_proto_->pieces()) {
    if (sp.type() == ModelProto::SentencePiece::NORMAL) {
      min_score_ = std::min(min_score_, sp.score());
      max_score_ = std::max(max_score_, sp.score());
    }
  }

  std::vector<std::pair<absl::string_view, int>> pieces;
  for (const auto &it : pieces_) pieces.emplace_back(it.first, it.second);

  BuildTrie(&pieces);
}

Model::~Model() {}

EncodeResult Model::Encode(absl::string_view normalized) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  EncodeResult results;
  for (const auto *node : lattice.Viterbi()) {
    results.emplace_back(node->piece, node->id);
  }

  return results;
}

NBestEncodeResult Model::NBestEncode(absl::string_view normalized,
                                     int nbest_size) const {
  if (!status().ok() || normalized.empty()) {
    return {{{}, 0.0}};
  }

  nbest_size = std::max<int>(1, std::min<int>(nbest_size, 1024));

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  NBestEncodeResult nbest_results;
  for (const auto &nbest : lattice.NBest(nbest_size)) {
    EncodeResult results;
    float score = 0.0;
    for (const auto *node : nbest) {
      score += node->score;
      results.emplace_back(node->piece, node->id);
    }
    nbest_results.emplace_back(results, score);
  }

  return nbest_results;
}

EncodeResult Model::SampleEncode(absl::string_view normalized,
                                 float theta) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  EncodeResult results;
  for (const auto *node : lattice.Sample(theta)) {
    results.emplace_back(node->piece, node->id);
  }

  return results;
}

}  // namespace unigram
}  // namespace sentencepiece
