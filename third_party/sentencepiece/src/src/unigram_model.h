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

#ifndef UNIGRAM_MODEL_H_
#define UNIGRAM_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common.h"
#include "src/freelist.h"
#include "src/model_interface.h"
#include "src/sentencepiece_model.pb.h"
#include "third_party/darts_clone/include/darts.h"

namespace sentencepiece {
namespace unigram {

// Lattice represents a search space of sentence piece segmentation.
class Lattice {
 public:
  Lattice();
  virtual ~Lattice();

  struct Node {
    absl::string_view piece;  // Sentence piece representation.
    uint32 pos;               // Unicode position in the sentence.
    uint32 length;            // Unicode length, not UT8 byte.
    uint32 node_id;           // unique id in the current lattice.
    int id;                   // vocab id. (maybe -1 for UNK)
    float score;              // logprob of this sentencepiece.
    float backtrace_score;    // backtrace info used in Viterbi.
    Node *prev;               // best previous node on Viterbi path.

    std::string DebugString() const;
  };

  // Returns bos node.
  Node *bos_node() const;

  // Returns eos node.
  Node *eos_node() const;

  // Returns nodes starting at |pos|.
  const std::vector<Node *> &begin_nodes(int pos) const;

  // Returns nodes ending at |pos|.
  const std::vector<Node *> &end_nodes(int pos) const;

  // Returns Unicode character length.
  int size() const;

  // Returns multi-byte (utf8) length.
  int utf8_size() const;

  // Returns the substring of sentence. sentence[pos:]
  const char *surface(int pos) const;

  // Returns immutable sentence. The same as surface(0)
  const char *sentence() const;

  // Clears the lattice.
  void Clear();

  // Sets new sentence.
  void SetSentence(absl::string_view sentence);

  // Inserts a new node at [pos, pos + length - 1].
  // After calling this method, The caller must set Node::score and Node::id.
  Node *Insert(int pos, int length);

  // Returns Viterbi path. All nodes must be populated in advance.
  std::vector<Node *> Viterbi();

  // Returns n-best results.
  std::vector<std::vector<Node *>> NBest(size_t nbest_size);

  // Samples one path from the lattice according to the
  // generation probability (Product of piece probabilities).
  // `theta` is a smoothing parameter.
  std::vector<Node *> Sample(float theta);

  // Populates marginal probability of every node in this lattice.
  // |freq| is the frequency of the sentence.
  //  for (auto *node : all_nodes_) {
  //    (*expected)[node->id] += marginal_prob_of_node * freq;
  //  }
  // Returns the log-likelihood of this sentence.
  float PopulateMarginal(float freq, std::vector<float> *expected) const;

 private:
  // Returns new node.
  // Lattice class has the ownership of the returned value.
  Node *NewNode();

  absl::string_view sentence_;
  std::vector<const char *> surface_;
  std::vector<std::vector<Node *>> begin_nodes_;
  std::vector<std::vector<Node *>> end_nodes_;
  model::FreeList<Node> node_allocator_;
};

class Model : public ModelInterface {
 public:
  explicit Model(const ModelProto &model_proto);
  Model() {}
  ~Model() override;

  EncodeResult Encode(absl::string_view normalized) const override;

  NBestEncodeResult NBestEncode(absl::string_view normalized,
                                int nbest_size) const override;

  EncodeResult SampleEncode(absl::string_view normalized,
                            float theta) const override;

  // Returns the minimum score in sentence pieces.
  // min_score() - 10 is used for the cost of unknown sentence.
  float min_score() const { return min_score_; }

  // Returns the maximum score in sentence pieces.
  // max_score() is used for the cost of user defined symbols.
  float max_score() const { return max_score_; }

  // Populates all sentence pieces to the |lattice|.
  // After calling this function, lattice.Viterbi() returns the
  // best segmentation.
  void PopulateNodes(Lattice *lattice) const;

  // Returns a vocab id of |piece|.
  int PieceToId(absl::string_view piece) const override;

 protected:
  // Builds a Trie index.
  void BuildTrie(std::vector<std::pair<absl::string_view, int>> *pieces);

  float min_score_ = 0.0;
  float max_score_ = 0.0;
  std::unique_ptr<Darts::DoubleArray> trie_;

  // Maximum size of the return value of Trie, which corresponds
  // to the maximum size of shared common prefix in the sentence pieces.
  int trie_results_size_;
};

}  // namespace unigram
}  // namespace sentencepiece
#endif  // UNIGRAM_MODEL_H_
