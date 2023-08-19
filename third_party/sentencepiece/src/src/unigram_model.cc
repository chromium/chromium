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

#include "unigram_model.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "util.h"

namespace sentencepiece {
namespace unigram {
namespace {

// Size of nodes pre-allocated in Lattice.
constexpr size_t kPreallocateLatticeNodeSize = 1024;

constexpr float kUnkPenalty = 10.0;
constexpr float kEpsilon = 1e-7;

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

// Returns a sample from a standard Gumbel distribution.
// If U  ~ U[0, 1], -log(-log U) ~ G(0,1)
inline float Gumbel() {
  const float kEpsilon = 1e-7;
  auto* mt = random::GetRandomGenerator();
  std::uniform_real_distribution<float> dis(0.0, 1.0);
  float noise = -std::log(-(std::log(dis(*mt) + kEpsilon)));

  return noise;
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

Lattice::LatticePathWithScore Lattice::Viterbi() {
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
  float score = begin_nodes(len)[0]->backtrace_score;
  for (Node *node = begin_nodes_[len][0]->prev; node->prev != nullptr;
       node = node->prev) {
    results.push_back(node);
  }

  std::reverse(results.begin(), results.end());

  LatticePathWithScore retval = {results, score};

  return retval;
}

std::vector<float> Lattice::ForwardAlgorithm(float inv_theta) const {
  const int len = size();
  std::vector<float> alpha(node_allocator_.size(), 0.0);

  for (int pos = 0; pos <= len; ++pos) {
    for (Node *rnode : begin_nodes_[pos]) {
      for (Node *lnode : end_nodes_[pos]) {
        alpha[rnode->node_id] =
            LogSumExp(alpha[rnode->node_id],
                      inv_theta * lnode->score + alpha[lnode->node_id],
                      lnode == end_nodes_[pos][0]);
      }
    }
  }

  return alpha;
}

std::vector<float> Lattice::BackwardAlgorithm(float inv_theta) const {
  const int len = size();
  std::vector<float> beta(node_allocator_.size(), 0.0);

  for (int pos = len; pos >= 0; --pos) {
    for (Node *lnode : end_nodes_[pos]) {
      for (Node *rnode : begin_nodes_[pos]) {
        beta[lnode->node_id] =
            LogSumExp(beta[lnode->node_id], rnode->score + beta[rnode->node_id],
                      rnode == begin_nodes_[pos][0]);
      }
    }
  }

  return beta;
}

float Lattice::PopulateMarginal(float freq,
                                std::vector<float>* expected) const {
  if (expected == nullptr) {
    return 0.0;
  }

  const int len = size();

  // alpha and beta (accumulative log prob) in Forward Backward.
  // the index of alpha/beta is Node::node_id.

  const auto alpha = ForwardAlgorithm(1.0);
  const auto beta = BackwardAlgorithm(1.0);

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

float Lattice::CalculateEntropy(float inv_theta) const {
  const int len = size();

  // alpha[node_id] is the marginal prob of sequence up to start of node
  // H is entropy of sequence
  // the index of alpha/H is Node::node_id.
  std::vector<float> H(node_allocator_.size(), 0.0);

  // Populate the forward marginals to get the normalising constant
  const auto alpha = ForwardAlgorithm(inv_theta);

  // Now populate the forward entropies
  for (int pos = 0; pos <= len; ++pos) {
    for (Node* rnode : begin_nodes_[pos]) {
      for (Node* lnode : end_nodes_[pos]) {
        // Contribution each lnode makes = p(lnode) * (H(lnode) + log p(lnode))

        // We have to normalise p(lnode) by the marginal contribution it makes
        const float lnode_transition_prob =
            ((inv_theta * lnode->score) + alpha[lnode->node_id] -
             alpha[rnode->node_id]);
        H[rnode->node_id] += std::exp(lnode_transition_prob) *
                             (H[lnode->node_id] + lnode_transition_prob);
      }
    }
  }

  return -H[begin_nodes_[len][0]->node_id];
}

namespace {

// The node structure to support A* algorithm in Lattice::NBest()
struct Hypothesis {
  Lattice::Node* node;
  Hypothesis* next;
  float fx;  // the priority to pop a new hypothesis from the priority queue.
  float gx;  // the sum of scores from EOS to the left-most node in x.
};

// Helper function for cloning a Hypothesis and the ones on their next paths.
// The graph structure is preserved.
//
//   to_clone:  the Hypothesis to clone.
//   clone_map: mapping between the old pointers and the new pointers.
//   allocator: allocate and own the cloned Hypothesis.
//
// Returns the cloned Hypothesis*. All Hypothesis on its "next" chain are also
// guaranteed to have been allocated via "allocator", and "clone_map" is updated
// with all new mappings.
Hypothesis* CloneHypAndDependents(
    const Hypothesis* to_clone,
    absl::flat_hash_map<const Hypothesis*, Hypothesis*>* clone_map,
    model::FreeList<Hypothesis>* allocator) {
  Hypothesis* cloned = nullptr;
  Hypothesis** result_callback = &cloned;

  // Iteratively clone "to_clone" and its dependencies.
  // The new pointer will be written back to *result_callback.
  while (to_clone != nullptr) {
    // If "to_clone" has already been cloned before, we just look up the result.
    auto iter = clone_map->find(to_clone);
    if (iter != clone_map->end()) {
      *result_callback = iter->second;
      break;
    }

    // Allocate a new Hypothesis and copy the values.
    Hypothesis* new_hyp = allocator->Allocate();
    *new_hyp = *to_clone;
    *result_callback = new_hyp;
    clone_map->insert({to_clone, new_hyp});

    // Move on to clone "to_clone->next".
    to_clone = to_clone->next;
    result_callback = &(new_hyp->next);
  }
  return cloned;
}

}  // namespace

std::vector<Lattice::LatticePathWithScore> Lattice::NBest(size_t nbest_size,
                                                          bool sample,
                                                          float inv_theta) {
  if (nbest_size < 1) {
    LOG(WARNING) << "nbest_size >= 1. Returns empty result.";
    return {};
  }

  if (nbest_size == 1 && !sample) {
    return {Viterbi()};
  }

  // Uses A* search to enumerate N-bests.
  // Given a lattice, enumerates hypotheses (paths) from EOS.
  // At each partial path x, compute f(x) as follows
  //   f(x) = g(x) + h(x).
  // g(x): the sum of scores from  EOS to the left-most node in x.
  //       for a complete hypothesis, g(hyp) is the score of the hypothesis.
  // h(x): a heuristic that estimates the largest score from x to BOS.
  // f(x): the priority to pop a new hypothesis from the priority queue.
  //
  // As left-to-right Viterbi search can tell the *exact* value of h(x),
  // we can obtain the exact n-best results with A*.

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
  std::vector<Lattice::LatticePathWithScore> results;

  auto *eos = hypothesis_allocator.Allocate();
  eos->node = eos_node();
  eos->next = nullptr;
  eos->gx = 0.0;

  std::vector<float> alpha(node_allocator_.size(), 0.0);

  if (sample) {
    // Run forwards algorithm to get normalising constants
    alpha = ForwardAlgorithm(inv_theta);
    // f(eos) = Gumbel(0), as it is the perturbed score of the entire lattice.
    eos->fx = Gumbel();
  } else {
    // Run Viterbi first to fill backtrace score.
    Viterbi();
    eos->fx = eos->node->backtrace_score;
  }
  agenda.push(eos);

  int shrink_count = 0;  // Number of times agenda has shrunk. For logging only.
  bool printed_memory_warning = false;  // For logging only.
  while (!agenda.empty()) {
    auto *top = agenda.top();
    agenda.pop();
    auto *node = top->node;

    // Reaches to BOS
    if (node == bos_node()) {
      results.resize(results.size() + 1);
      for (auto *n = top->next; n->next != nullptr; n = n->next) {
        results.back().first.push_back(n->node);
      }
      results.back().second = top->fx;
      if (results.size() == nbest_size) {
        break;
      }
      continue;
    }

    const int end_nodes_size = end_nodes(node->pos).size();
    std::vector<float> probs(end_nodes_size, 0.0);
    std::vector<float> perturbed_probs(end_nodes_size, 0.0);
    std::vector<double> adjusted_probs(end_nodes_size, 0.0);
    const float Z = alpha[node->node_id];
    if (sample) {
      float max_score = -1e8;
      // Calculate the marginal and perturbed scores for stochastic search
      for (int i = 0; i < end_nodes(node->pos).size(); i++) {
        Node* lnode = end_nodes(node->pos)[i];
        // Calculate backwards transition score
        probs[i] =
            top->gx + alpha[lnode->node_id] + (inv_theta * lnode->score) - Z;
        perturbed_probs[i] = probs[i] + Gumbel();
        if (perturbed_probs[i] > max_score) {
          max_score = perturbed_probs[i];
        }
      }
      // Now constrain the sampled continuations to match the score of parent
      for (int i = 0; i < adjusted_probs.size(); i++) {
        // Use numerically stable version of truncated Gumbel:
        // https://arxiv.org/pdf/1903.06059.pdf appendix B.3
        const float v = top->fx - perturbed_probs[i] +
                        std::log1p(-std::exp(perturbed_probs[i] - max_score));
        adjusted_probs[i] = top->fx - std::max(static_cast<float>(0.0), v) -
                            std::log1p(std::exp(-std::abs(v)));
      }
    }

    // Expands new node ending at node->pos
    for (int i = 0; i < end_nodes(node->pos).size(); i++) {
      Node* lnode = end_nodes(node->pos)[i];
      auto *hyp = hypothesis_allocator.Allocate();
      hyp->node = lnode;
      if (sample) {
        hyp->gx = probs[i];
        hyp->fx = adjusted_probs[i];
      } else {
        hyp->gx = lnode->score + top->gx;  // just adds node->score
        hyp->fx =
            lnode->backtrace_score + top->gx;  // backtrace_score is h(node).
      }
      hyp->next = top;
      agenda.push(hyp);
    }

    static constexpr int kOneBillion = 1000000000;  // 10^9.
    if (hypothesis_allocator.size() >= kOneBillion) {
      if (!printed_memory_warning) {
        printed_memory_warning = true;
        LOG(WARNING) << "Allocator size exceeds " << kOneBillion
                     << " with an example of length " << this->size();
      }
    }

    // When the input is too long or contains duplicated phrases,
    // `agenda` will get extremely big. Here we avoid this case by
    // dynamically shrinking the agenda.
    constexpr int kMaxAgendaSize = 10000;
    constexpr int kMinAgendaSize = 512;
    if (agenda.size() >= kMaxAgendaSize) {
      // Keeps the top `kMinAgendaSize` hypothesis.
      Agenda new_agenda;
      // Keeps the top hypothesis and the ones on their "next" paths.
      model::FreeList<Hypothesis> new_allocator(kPreallocatedHypothesisSize);
      // Map between old Hypothesis* and new Hypothesis*.
      absl::flat_hash_map<const Hypothesis*, Hypothesis*> clone_map;

      const int size = std::min<int>(kMinAgendaSize, nbest_size * 10);
      shrink_count++;
      LOG(WARNING) << "Too big agenda size " << agenda.size()
                   << ". Shrinking (round " << shrink_count << ") down to "
                   << size << ".";
      for (int i = 0; i < size; ++i) {
        const Hypothesis* top_hyp = agenda.top();
        Hypothesis* cloned_hyp =
            CloneHypAndDependents(top_hyp, &clone_map, &new_allocator);
        new_agenda.push(cloned_hyp);
        agenda.pop();
      }
      agenda = std::move(new_agenda);
      hypothesis_allocator.swap(new_allocator);
    }
  }

  return results;
}

std::vector<Lattice::Node*> Lattice::Sample(float inv_theta) {
  const int len = size();
  if (len == 0) return {};

  std::vector<float> alpha(node_allocator_.size(), 0.0);

  alpha = ForwardAlgorithm(inv_theta);

  auto *mt = random::GetRandomGenerator();

  std::vector<Node *> results;
  std::vector<float> probs;

  float Z = alpha[eos_node()->node_id];
  Node *node = eos_node();
  while (true) {
    probs.clear();
    for (const Node *lnode : end_nodes_[node->pos]) {
      probs.push_back(std::exp(static_cast<double>(
          alpha[lnode->node_id] + inv_theta * lnode->score - Z)));
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
      node->score = IsUserDefinedInlined(id) ? (length * max_score_ - 0.1)
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
  trie_->exactMatchSearch(piece.data(), id, piece.size());
  return id == -1 ? unk_id_ : id;
}

void Model::BuildTrie(std::vector<std::pair<absl::string_view, int>> *pieces) {
  if (!status().ok()) return;

  if (pieces->empty()) {
    status_ = util::InternalError("no pieces are loaded.");
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
    status_ = util::InternalError("cannot build double-array.");
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
    status_ = util::InternalError("no entry is found in the trie.");
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
  if (encoder_version_ == EncoderVersion::kOptimized) {
    return EncodeOptimized(normalized);
  }

  if (!status().ok() || normalized.empty()) {
    return {};
  }

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  EncodeResult results;
  for (const auto* node : lattice.Viterbi().first) {
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

  if (nbest_size <= 1) {
    return {std::pair<EncodeResult, float>(Encode(normalized), 0.0)};
  }

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  NBestEncodeResult nbest_results;
  for (const auto& nbest : lattice.NBest(nbest_size, false, 0.0)) {
    EncodeResult results;
    for (const auto* node : nbest.first) {
      results.emplace_back(node->piece, node->id);
    }
    nbest_results.emplace_back(results, nbest.second);
  }

  return nbest_results;
}

EncodeResult Model::SampleEncode(absl::string_view normalized,
                                 float inv_theta) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }

  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  EncodeResult results;
  for (const auto* node : lattice.Sample(inv_theta)) {
    results.emplace_back(node->piece, node->id);
  }

  return results;
}

NBestEncodeResult Model::SampleEncodeAndScore(absl::string_view normalized,
                                              float inv_theta,
                                              int samples,
                                              bool wor,
                                              bool include_best) const {
  if (!status().ok() || normalized.empty()) {
    return {};
  }
  NBestEncodeResult results;
  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  const std::vector<float> alpha = lattice.ForwardAlgorithm(inv_theta);
  const float marginal = alpha[lattice.eos_node()->node_id];

  if (include_best) {
    if (!wor) {
      LOG(ERROR) << "include_best not supported for wor false";
      return {};
    }
    EncodeResult result;
    const auto best_path = lattice.Viterbi();
    for (const auto* node : best_path.first) {
      result.emplace_back(node->piece, node->id);
    }

    // Inclusion probability if we always include the best is 1.
    results.emplace_back(result, 0.0);
  }

  if (wor) {
    // Draw k+1 samples as we need perturbed score of k+1th element
    auto nbest_samples = lattice.NBest(samples + 1, true, inv_theta);

    if (include_best) {
      std::vector<std::vector<Lattice::Node*>> nbest_paths(
          nbest_samples.size());
      for (int i = 0; i < nbest_samples.size(); i++) {
        nbest_paths[i] = nbest_samples[i].first;
      }
      // Remove the best result from the samples if necessary
      const auto best_path = lattice.Viterbi();

      const int index_of_best =
          (std::find(nbest_paths.begin(), nbest_paths.end(), best_path.first) -
           nbest_paths.begin());

      if (index_of_best != nbest_samples.size()) {
        nbest_samples.erase(nbest_samples.begin() + index_of_best);
      } else {
        nbest_samples.pop_back();
      }
    }
    // We use the perturbed score of the k+1th element to calculate the
    // inclusion probability.
    const double kappa = static_cast<double>(nbest_samples.back().second);
    // Discard the last sample
    nbest_samples.pop_back();
    for (const auto& nbest : nbest_samples) {
      EncodeResult result;
      float score = 0.0;

      for (const auto* node : nbest.first) {
        score += (inv_theta * node->score);
        result.emplace_back(node->piece, node->id);
      }

      results.emplace_back(result, score - marginal);
    }

    // Now calculate the inclusion probability
    for (auto& it : results) {
      // Only modify non best sample inclusion probabilities.
      if (it.second != 0.0) {
        const double x = it.second - kappa;
        const double y = std::exp(x);
        double inclusion_prob;
        if (x <= -10) {
          // Series expansion of the log Gumbel survival function up to eps.
          inclusion_prob =
              x - (y / 2) + (std::pow(y, 2) / 24) - std::pow(y, 4) / 2880;
        } else {
          inclusion_prob = std::log(-std::expm1(-y));
        }
        it.second = static_cast<float>(inclusion_prob);
      }
    }
  } else {
    while (results.size() < samples) {
      Lattice lattice;
      lattice.SetSentence(normalized);
      PopulateNodes(&lattice);

      float score = 0.0;
      EncodeResult result;
      const std::vector<Lattice::Node*> sample = lattice.Sample(inv_theta);
      for (const auto* node : sample) {
        result.emplace_back(node->piece, node->id);
        score += (inv_theta * node->score);
      }
      results.emplace_back(result, score - marginal);
    }
  }

  return results;
}

float Model::CalculateEntropy(absl::string_view normalized,
                              float inv_theta) const {
  Lattice lattice;
  lattice.SetSentence(normalized);
  PopulateNodes(&lattice);

  return lattice.CalculateEntropy(inv_theta);
}

bool Model::VerifyOutputsEquivalent(absl::string_view expected,
                                    absl::string_view actual) const {
  auto compute_unigram_model_score =
      [this](std::vector<absl::string_view> output_pieces) {
        float total_score = 0;
        const float unk_score = min_score() - kUnkPenalty;
        for (const auto p : output_pieces) {
          const auto id = PieceToId(p);
          if (id == unk_id_) {
            total_score += unk_score;
          } else {
            const int length = p.size();
            total_score += IsUserDefinedInlined(id)
                               ? (length * max_score_ - 0.1)
                               : GetScoreInlined(id);
          }
        }
        return total_score;
      };
  const auto expected_score =
      compute_unigram_model_score(absl::StrSplit(expected, ' '));
  const auto actual_score =
      compute_unigram_model_score(absl::StrSplit(actual, ' '));
  if (std::abs(expected_score - actual_score) > kEpsilon) {
    LOG(WARNING) << "Two sentence piece sequences are not equivalent! Left: "
                 << expected << ", Score: " << expected_score
                 << ". Right: " << actual << ", Score: " << actual_score << ".";
    return false;
  }
  return true;
}

EncodeResult Model::EncodeOptimized(absl::string_view normalized) const {
  // An optimized Viterbi algorithm for unigram language models. Benchmarking
  // results show that it generates almost identical outputs and achieves 2.1x
  // speedup on average for 102 languages compared to the original
  // implementation. It's based on the following three ideas:
  //
  // 1. Because it uses the *unigram* model:
  //     best_score(x1, x2, …, xt) = best_score(x1, x2, …, x{t-1}) + score(xt)
  // Deciding the best path (and score) can be decoupled into two isolated
  // terms: (a) the best path ended before the last token `best_score(x1, x2, …,
  // x{t-1})`, and (b) the last token and its `score(xt)`. The two terms are
  // not related to each other at all.
  //
  // Therefore, we can compute once and store the *best_path ending at
  // each character position*. In this way, when we know best_path_ends_at[M],
  // we can reuse it to compute all the best_path_ends_at_[...] where the last
  // token starts at the same character position M.
  //
  // This improves the time complexity from O(n*k*k) to O(n*k) because it
  // eliminates the extra loop of recomputing the best path ending at the same
  // position, where n is the input length and k is the maximum number of tokens
  // that can be recognized starting at each position.
  //
  // 2. Again, because it uses the *unigram* model, we don’t need to actually
  // store the lattice nodes. We still recognize all the tokens and lattice
  // nodes from the input, but along identifying them, we use and discard them
  // on the fly. There is no need to actually store them for best path Viterbi
  // decoding. The only thing we need to store is the best_path ending at
  // each character position.
  //
  // This improvement reduces the things needed to store in memory from O(n*k)
  // to O(n), where n is the input length and k is the maximum number of tokens
  // that can be recognized starting at each position.
  //
  // It also avoids the need of dynamic-size lattice node pool, because the
  // number of things to store is fixed as n.
  //
  // 3. SentencePiece is designed to work with unicode, taking utf-8 encoding
  // inputs. In the original implementation, the lattice positions are based on
  // unicode positions. A mapping from unicode position to the utf-8 position is
  // maintained to recover the utf-8 string piece.
  //
  // We found that it is sufficient and beneficial to directly work with utf-8
  // positions:
  //
  // Firstly, it saves the conversion and mapping between unicode positions and
  // utf-8 positions.
  //
  // Secondly, it reduces the number of fields we need to maintain in the
  // node/path structure. Specifically, there are 8 fields defined in
  // `Lattice::Node` used by the original encoder, but here in the optimized
  // encoder we only need to define 3 fields in `BestPathNode`.

  if (!status().ok() || normalized.empty()) {
    return {};
  }
  // Represents the last node of the best path.
  struct BestPathNode {
    int id = -1;  // The vocab id. (maybe -1 for UNK)
    float best_path_score =
        0;  // The total score of the best path ending at this node.
    int starts_at =
        -1;  // The starting position (in utf-8) of this node. The entire best
             // path can be constructed by backtracking along this link.
  };
  const int size = normalized.size();
  const float unk_score = min_score() - kUnkPenalty;
  // The ends are exclusive.
  std::vector<BestPathNode> best_path_ends_at(size + 1);
  // Generate lattice on-the-fly (not stored) and update best_path_ends_at.
  int starts_at = 0;
  while (starts_at < size) {
    std::size_t node_pos = 0;
    std::size_t key_pos = starts_at;
    const auto best_path_score_till_here =
        best_path_ends_at[starts_at].best_path_score;
    bool has_single_node = false;
    const int mblen =
        std::min<int>(string_util::OneCharLen(normalized.data() + starts_at),
                      size - starts_at);
    while (key_pos < size) {
      const int ret =
          trie_->traverse(normalized.data(), node_pos, key_pos, key_pos + 1);
      if (ret == -2) {
        break;
      }
      if (ret >= 0) {
        if (IsUnusedInlined(ret)) {
          continue;
        }
        // Update the best path node.
        auto& target_node = best_path_ends_at[key_pos];
        const auto length = (key_pos - starts_at);
        // User defined symbol receives extra bonus to always be selected.
        const auto score = IsUserDefinedInlined(ret)
                               ? (length * max_score_ - 0.1)
                               : GetScoreInlined(ret);
        const auto candidate_best_path_score =
            score + best_path_score_till_here;
        if (target_node.starts_at == -1 ||
            candidate_best_path_score > target_node.best_path_score) {
          target_node.best_path_score = candidate_best_path_score;
          target_node.starts_at = starts_at;
          target_node.id = ret;
        }
        if (!has_single_node && length == mblen) {
          has_single_node = true;
        }
      }
    }
    if (!has_single_node) {
      auto& target_node = best_path_ends_at[starts_at + mblen];
      const auto candidate_best_path_score =
          unk_score + best_path_score_till_here;
      if (target_node.starts_at == -1 ||
          candidate_best_path_score > target_node.best_path_score) {
        target_node.best_path_score = candidate_best_path_score;
        target_node.starts_at = starts_at;
        target_node.id = unk_id_;
      }
    }
    // Move by one unicode character.
    starts_at += mblen;
  }
  // Backtrack to identify the best path.
  EncodeResult results;
  int ends_at = size;
  while (ends_at > 0) {
    const auto& node = best_path_ends_at[ends_at];
    results.emplace_back(
        normalized.substr(node.starts_at, ends_at - node.starts_at), node.id);
    ends_at = node.starts_at;
  }
  std::reverse(results.begin(), results.end());
  return results;
}
}  // namespace unigram
}  // namespace sentencepiece
