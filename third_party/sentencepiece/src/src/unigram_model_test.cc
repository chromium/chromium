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

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "sentencepiece_model.pb.h"
#include "sentencepiece_processor.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace unigram {

TEST(LatticeTest, SetSentenceTest) {
  Lattice lattice;

  EXPECT_EQ(0, lattice.size());
  EXPECT_EQ(0, lattice.utf8_size());

  lattice.SetSentence("");
  EXPECT_EQ(0, lattice.size());
  EXPECT_EQ(0, lattice.utf8_size());
  EXPECT_STREQ("", lattice.sentence());
  EXPECT_STREQ("", lattice.surface(0));

  lattice.SetSentence("test");
  EXPECT_EQ(4, lattice.size());
  EXPECT_EQ(4, lattice.utf8_size());
  EXPECT_STREQ("test", lattice.sentence());
  EXPECT_STREQ("test", lattice.surface(0));
  EXPECT_STREQ("est", lattice.surface(1));
  EXPECT_STREQ("st", lattice.surface(2));
  EXPECT_STREQ("t", lattice.surface(3));

  Lattice::Node *bos = lattice.bos_node();
  Lattice::Node *eos = lattice.eos_node();

  EXPECT_EQ(-1, bos->id);
  EXPECT_EQ(-1, eos->id);
  EXPECT_EQ(bos, lattice.end_nodes(0).front());
  EXPECT_EQ(eos, lattice.begin_nodes(4).front());

  lattice.SetSentence("テストab");
  EXPECT_EQ(5, lattice.size());
  EXPECT_EQ(11, lattice.utf8_size());
  EXPECT_STREQ("テストab", lattice.sentence());
  EXPECT_STREQ("テストab", lattice.surface(0));
  EXPECT_STREQ("ストab", lattice.surface(1));
  EXPECT_STREQ("トab", lattice.surface(2));
  EXPECT_STREQ("ab", lattice.surface(3));
  EXPECT_STREQ("b", lattice.surface(4));

  lattice.Clear();
  EXPECT_EQ(0, lattice.size());
  EXPECT_EQ(0, lattice.utf8_size());
}

TEST(LatticeTest, InsertTest) {
  Lattice lattice;
  lattice.SetSentence("ABあい");

  Lattice::Node *node[7];
  node[0] = lattice.Insert(0, 1);
  node[1] = lattice.Insert(1, 1);
  node[2] = lattice.Insert(2, 1);
  node[3] = lattice.Insert(3, 1);
  node[4] = lattice.Insert(0, 2);
  node[5] = lattice.Insert(1, 2);
  node[6] = lattice.Insert(2, 2);

  EXPECT_EQ("A", node[0]->piece);
  EXPECT_EQ("B", node[1]->piece);
  EXPECT_EQ("あ", node[2]->piece);
  EXPECT_EQ("い", node[3]->piece);
  EXPECT_EQ("AB", node[4]->piece);
  EXPECT_EQ("Bあ", node[5]->piece);
  EXPECT_EQ("あい", node[6]->piece);

  EXPECT_EQ("A", node[0]->piece);
  EXPECT_EQ("B", node[1]->piece);
  EXPECT_EQ("あ", node[2]->piece);
  EXPECT_EQ("い", node[3]->piece);
  EXPECT_EQ("AB", node[4]->piece);
  EXPECT_EQ("Bあ", node[5]->piece);
  EXPECT_EQ("あい", node[6]->piece);

  EXPECT_EQ(0, node[0]->pos);
  EXPECT_EQ(1, node[1]->pos);
  EXPECT_EQ(2, node[2]->pos);
  EXPECT_EQ(3, node[3]->pos);
  EXPECT_EQ(0, node[4]->pos);
  EXPECT_EQ(1, node[5]->pos);
  EXPECT_EQ(2, node[6]->pos);

  EXPECT_EQ(1, node[0]->length);
  EXPECT_EQ(1, node[1]->length);
  EXPECT_EQ(1, node[2]->length);
  EXPECT_EQ(1, node[3]->length);
  EXPECT_EQ(2, node[4]->length);
  EXPECT_EQ(2, node[5]->length);
  EXPECT_EQ(2, node[6]->length);

  EXPECT_EQ(0, lattice.bos_node()->node_id);
  EXPECT_EQ(1, lattice.eos_node()->node_id);
  EXPECT_EQ(2, node[0]->node_id);
  EXPECT_EQ(3, node[1]->node_id);
  EXPECT_EQ(4, node[2]->node_id);
  EXPECT_EQ(5, node[3]->node_id);
  EXPECT_EQ(6, node[4]->node_id);
  EXPECT_EQ(7, node[5]->node_id);
  EXPECT_EQ(8, node[6]->node_id);

  EXPECT_EQ(2, lattice.begin_nodes(0).size());
  EXPECT_EQ(2, lattice.begin_nodes(1).size());
  EXPECT_EQ(2, lattice.begin_nodes(2).size());
  EXPECT_EQ(1, lattice.begin_nodes(3).size());
  EXPECT_EQ(1, lattice.begin_nodes(4).size());  // EOS

  EXPECT_EQ(1, lattice.end_nodes(0).size());  // BOS
  EXPECT_EQ(1, lattice.end_nodes(1).size());
  EXPECT_EQ(2, lattice.end_nodes(2).size());
  EXPECT_EQ(2, lattice.end_nodes(3).size());
  EXPECT_EQ(2, lattice.end_nodes(4).size());

  EXPECT_EQ(node[0], lattice.begin_nodes(0)[0]);
  EXPECT_EQ(node[4], lattice.begin_nodes(0)[1]);
  EXPECT_EQ(node[1], lattice.begin_nodes(1)[0]);
  EXPECT_EQ(node[5], lattice.begin_nodes(1)[1]);
  EXPECT_EQ(node[2], lattice.begin_nodes(2)[0]);
  EXPECT_EQ(node[6], lattice.begin_nodes(2)[1]);
  EXPECT_EQ(node[3], lattice.begin_nodes(3)[0]);
  EXPECT_EQ(lattice.eos_node(), lattice.begin_nodes(4)[0]);

  EXPECT_EQ(lattice.bos_node(), lattice.end_nodes(0)[0]);
  EXPECT_EQ(node[0], lattice.end_nodes(1)[0]);
  EXPECT_EQ(node[1], lattice.end_nodes(2)[0]);
  EXPECT_EQ(node[4], lattice.end_nodes(2)[1]);
  EXPECT_EQ(node[2], lattice.end_nodes(3)[0]);
  EXPECT_EQ(node[5], lattice.end_nodes(3)[1]);
  EXPECT_EQ(node[3], lattice.end_nodes(4)[0]);
  EXPECT_EQ(node[6], lattice.end_nodes(4)[1]);
}

TEST(LatticeTest, ViterbiFromIncompleteLatticeTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");
  EXPECT_TRUE(lattice.Viterbi().first.empty());

  // Still incomplete
  lattice.Insert(0, 1);
  EXPECT_TRUE(lattice.Viterbi().first.empty());

  lattice.Insert(1, 1);
  lattice.Insert(2, 1);
  lattice.Viterbi();
}

std::string GetTokenized(const std::vector<Lattice::Node *> &nodes) {
  std::vector<std::string> tokens;
  for (auto *node : nodes) {
    tokens.push_back(std::string(node->piece));
  }
  return absl::StrJoin(tokens, " ");
}

void InsertWithScore(Lattice *lattice, int pos, int length, float score) {
  lattice->Insert(pos, length)->score = score;
}

void InsertWithScoreAndId(Lattice *lattice, int pos, int length, float score,
                          int id) {
  auto *node = lattice->Insert(pos, length);
  node->score = score;
  node->id = id;
}

TEST(LatticeTest, ViterbiTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScore(&lattice, 0, 1, 0.0);  // A
  InsertWithScore(&lattice, 1, 1, 0.0);  // B
  InsertWithScore(&lattice, 2, 1, 0.0);  // C
  EXPECT_EQ("A B C", GetTokenized(lattice.Viterbi().first));

  InsertWithScore(&lattice, 0, 2, 2.0);  // AB
  EXPECT_EQ("AB C", GetTokenized(lattice.Viterbi().first));

  InsertWithScore(&lattice, 1, 2, 5.0);  // BC
  EXPECT_EQ("A BC", GetTokenized(lattice.Viterbi().first));

  InsertWithScore(&lattice, 0, 3, 10.0);  // ABC
  EXPECT_EQ("ABC", GetTokenized(lattice.Viterbi().first));
}

TEST(LatticeTest, NBestTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScore(&lattice, 0, 1, 0.0);   // A
  InsertWithScore(&lattice, 1, 1, 0.0);   // B
  InsertWithScore(&lattice, 2, 1, 0.0);   // C
  InsertWithScore(&lattice, 0, 2, 2.0);   // AB
  InsertWithScore(&lattice, 1, 2, 5.0);   // BC
  InsertWithScore(&lattice, 0, 3, 10.0);  // ABC

  auto nbests = lattice.NBest(10, false, 0.0);
  EXPECT_EQ(4, nbests.size());

  EXPECT_EQ("ABC", GetTokenized(nbests[0].first));
  EXPECT_EQ("A BC", GetTokenized(nbests[1].first));
  EXPECT_EQ("AB C", GetTokenized(nbests[2].first));
  EXPECT_EQ("A B C", GetTokenized(nbests[3].first));

  auto nbests0 = lattice.NBest(0, false, 0.0);
  EXPECT_TRUE(nbests0.empty());

  auto nbests1 = lattice.NBest(1, false, 0.0);
  EXPECT_EQ(nbests1.size(), 1);
}

TEST(LatticeTest, NBestSampleTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScore(&lattice, 0, 1, 0.0);  // A
  InsertWithScore(&lattice, 1, 1, 0.0);  // B
  InsertWithScore(&lattice, 2, 1, 0.1);  // C
  InsertWithScore(&lattice, 0, 2, 0.2);  // AB
  InsertWithScore(&lattice, 1, 2, 0.5);  // BC
  InsertWithScore(&lattice, 0, 3, 1.0);  // ABC

  // Calculate expected probabilities of each path
  // Note that sampling without replacement affects the expected frequencies!
  const std::vector<double> kInv_Theta = {0.0, 0.01, 0.5, 0.7, 1.0};
  for (const auto inv_theta : kInv_Theta) {
    std::vector<std::string> strings = {"ABC", "AB C", "A BC", "A B C"};
    std::map<std::string, float> probs;
    probs["ABC"] = std::exp(inv_theta * 1.0);
    probs["AB C"] = std::exp(inv_theta * (0.2 + 0.1));
    probs["A BC"] = std::exp(inv_theta * (0.0 + 0.5));
    probs["A B C"] = std::exp(inv_theta * (0.0 + 0.0 + 0.1));

    for (const auto& it : strings) {
      EXPECT_EQ(1, probs.count(it));
    }

    double Z = 0.0;
    for (const auto& it : probs) {
      Z += it.second;
    }
    for (auto& it : probs) {
      it.second /= Z;
    }

    std::map<std::pair<std::string, std::string>, float> pair_probs;
    for (const auto& first : strings) {
      for (const auto& second : strings) {
        if (first == second) {
          pair_probs[std::make_pair(first, second)] = 0;
        } else {
          float first_prob = probs[first];
          float second_prob = probs[second] / (1 - first_prob);
          pair_probs[std::make_pair(first, second)] = first_prob * second_prob;
        }
      }
    }

    std::map<std::string, float> inclusion_probs;
    for (const auto& string : strings) {
      float inclusion_prob = 0.0;
      for (const auto& other_string : strings) {
        inclusion_prob += pair_probs[std::make_pair(string, other_string)];
      }
      for (const auto& other_string : strings) {
        inclusion_prob += pair_probs[std::make_pair(other_string, string)];
      }
      inclusion_probs[string] = inclusion_prob / 2;
    }

    int kTrials = 10000;

    std::vector<int> kNumSamples = {1, 2};

    for (const auto num_samples : kNumSamples) {
      std::map<std::string, int> counts;
      for (int i = 0; i < kTrials; i++) {
        auto nbests = lattice.NBest(num_samples, true, inv_theta);
        for (const auto& nbest : nbests) {
          counts[GetTokenized(nbest.first)]++;
        }
      }

      EXPECT_EQ(inclusion_probs.size(), counts.size());
      // If we take multiple samples WOR, we have to use corrected probs.
      std::map<std::string, float> probs_to_use =
          (num_samples == 1 ? probs : inclusion_probs);

      for (const auto& it : probs_to_use) {
        EXPECT_NEAR(it.second, 1.0 * counts[it.first] / (kTrials * num_samples),
                    0.02);
      }
    }
  }
}

TEST(LatticeTest, CalculateEntropyTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScore(&lattice, 0, 1, 0.0);  // A
  InsertWithScore(&lattice, 1, 1, 0.0);  // B
  InsertWithScore(&lattice, 2, 1, 0.1);  // C
  InsertWithScore(&lattice, 0, 2, 0.2);  // AB
  InsertWithScore(&lattice, 1, 2, 0.5);  // BC
  InsertWithScore(&lattice, 0, 3, 1.0);  // ABC

  // Calculate expected probabilities of each path
  const std::vector<double> kInv_Theta = {0.0, 0.01, 0.5, 0.7, 1.0};
  for (const auto inv_theta : kInv_Theta) {
    std::vector<std::string> strings = {"ABC", "AB C", "A BC", "A B C"};
    std::map<std::string, float> probs;
    probs["ABC"] = std::exp(inv_theta * 1.0);
    probs["AB C"] = std::exp(inv_theta * (0.2 + 0.1));
    probs["A BC"] = std::exp(inv_theta * (0.0 + 0.5));
    probs["A B C"] = std::exp(inv_theta * (0.0 + 0.0 + 0.1));

    double Z = 0.0;
    for (const auto& it : probs) {
      Z += it.second;
    }
    for (auto& it : probs) {
      it.second /= Z;
    }

    for (const auto& it : strings) {
      EXPECT_EQ(1, probs.count(it));
    }
    float entropy = 0.0;
    for (const auto& it : probs) {
      entropy += (it.second * std::log(it.second));
    }
    EXPECT_NEAR(-entropy, lattice.CalculateEntropy(inv_theta), 0.02);
  }
}

TEST(LatticeTest, ForwardAlgorithmTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScore(&lattice, 0, 1, 0.0);  // A
  InsertWithScore(&lattice, 1, 1, 0.0);  // B
  InsertWithScore(&lattice, 2, 1, 0.1);  // C
  InsertWithScore(&lattice, 0, 2, 0.2);  // AB
  InsertWithScore(&lattice, 1, 2, 0.5);  // BC
  InsertWithScore(&lattice, 0, 3, 1.0);  // ABC

  const std::vector<float> kInv_Theta = {0.0, 0.01, 0.5, 0.7, 1.0};
  for (const auto inv_theta : kInv_Theta) {
    std::vector<float> alpha = lattice.ForwardAlgorithm(inv_theta);
    EXPECT_EQ(alpha.size(), 8);  // 6 nodes, plus BOS, EOS
    // only alpha[C], alpha[EOS] have non-zero alpha
    for (int i : {0, 1, 2, 3}) {
      for (const auto& node : lattice.begin_nodes(i)) {
        if (i < 2) {
          EXPECT_EQ(alpha[node->node_id], 0.0);
        } else if (i == 2) {
          float Z = std::log(std::exp(inv_theta * (0.0 + 0.0)) +
                             std::exp(inv_theta * 0.2));
          EXPECT_EQ(alpha[node->node_id], Z);
        } else if (i == 3) {
          float Z =
              std::log(std::exp(inv_theta * (0.0 + 0.0 + 0.1)) +  // A + B + C
                       std::exp(inv_theta * (0.2 + 0.1)) +        // AB + C
                       std::exp(inv_theta * (0.0 + 0.5)) +        // A + BC
                       std::exp(inv_theta * 1.0));                // ABC
          EXPECT_EQ(Z, alpha[node->node_id]);
        }
      }
    }
  }
}

TEST(LatticeTest, PopulateMarginalTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScoreAndId(&lattice, 0, 1, 1.0, 0);  // A
  InsertWithScoreAndId(&lattice, 1, 1, 1.2, 1);  // B
  InsertWithScoreAndId(&lattice, 2, 1, 2.5, 2);  // C
  InsertWithScoreAndId(&lattice, 0, 2, 3.0, 3);  // AB
  InsertWithScoreAndId(&lattice, 1, 2, 4.0, 4);  // BC
  InsertWithScoreAndId(&lattice, 0, 3, 2.0, 5);  // ABC

  std::vector<float> probs(6, 0.0);

  // Expand all paths:
  // A B C : exp(1.0 + 1.2 + 2.5) => path1
  // AB  C : exp(3.0 + 2.5)       => path2
  // A  BC : exp(1.0 + 4.0)       => path3
  // ABC   : exp(2.0)             => path4
  const float p1 = exp(1.0 + 1.2 + 2.5);
  const float p2 = exp(3.0 + 2.5);
  const float p3 = exp(1.0 + 4.0);
  const float p4 = exp(2.0);
  const float Z = p1 + p2 + p3 + p4;

  const float logZ = lattice.PopulateMarginal(1.0, &probs);

  EXPECT_NEAR((p1 + p3) / Z, probs[0], 0.001);  // A
  EXPECT_NEAR(p1 / Z, probs[1], 0.001);         // B
  EXPECT_NEAR((p1 + p2) / Z, probs[2], 0.001);  // C
  EXPECT_NEAR(p2 / Z, probs[3], 0.001);         // AB
  EXPECT_NEAR(p3 / Z, probs[4], 0.001);         // BC
  EXPECT_NEAR(p4 / Z, probs[5], 0.001);         // ABC
  EXPECT_NEAR(std::log(static_cast<double>(Z)), logZ, 0.001);
}

TEST(LatticeTest, SampleTest) {
  Lattice lattice;
  lattice.SetSentence("ABC");

  InsertWithScoreAndId(&lattice, 0, 1, 1.0, 0);  // A
  InsertWithScoreAndId(&lattice, 1, 1, 1.2, 1);  // B
  InsertWithScoreAndId(&lattice, 2, 1, 1.5, 2);  // C
  InsertWithScoreAndId(&lattice, 0, 2, 1.6, 3);  // AB
  InsertWithScoreAndId(&lattice, 1, 2, 1.7, 4);  // BC
  InsertWithScoreAndId(&lattice, 0, 3, 1.8, 5);  // ABC

  const std::vector<double> kInv_Theta = {0.0, 0.01, 0.5, 0.7, 1.0};
  for (int i = 0; i < kInv_Theta.size(); ++i) {
    std::map<std::string, double> probs;
    // Expands all paths in the lattice.
    probs["A B C"] = exp(kInv_Theta[i] * (1.0 + 1.2 + 1.5));  // A B C
    probs["AB C"] = exp(kInv_Theta[i] * (1.6 + 1.5));         // AB C
    probs["A BC"] = exp(kInv_Theta[i] * (1.0 + 1.7));         // A BC
    probs["ABC"] = exp(kInv_Theta[i] * 1.8);                  // ABC

    // Computes expected probabilities.
    double Z = 0.0;
    for (const auto &it : probs) Z += it.second;
    for (auto &it : probs) it.second /= Z;

    // Samples `kTrial` times and verifies the probabilities.
    constexpr int kTrial = 100000;
    std::map<std::string, int> freq;
    for (int n = 0; n < kTrial; ++n) {
      freq[GetTokenized(lattice.Sample(kInv_Theta[i]))]++;
    }

    EXPECT_EQ(probs.size(), freq.size());
    for (const auto &it : probs) {
      EXPECT_NEAR(it.second, 1.0 * freq[it.first] / kTrial, 0.02);
    }
  }
}

ModelProto MakeBaseModelProto() {
  ModelProto model_proto;
  auto *sp1 = model_proto.add_pieces();
  auto *sp2 = model_proto.add_pieces();
  auto *sp3 = model_proto.add_pieces();

  sp1->set_type(ModelProto::SentencePiece::UNKNOWN);
  sp1->set_piece("<unk>");
  sp2->set_type(ModelProto::SentencePiece::CONTROL);
  sp2->set_piece("<s>");
  sp3->set_type(ModelProto::SentencePiece::CONTROL);
  sp3->set_piece("</s>");

  return model_proto;
}

// Returns model protos in parameterized tests.
const std::vector<Model::EncoderVersion>& GetEncoderVersions() {
  static const std::vector<Model::EncoderVersion>& v =
      *new std::vector<Model::EncoderVersion>{Model::kOptimized,
                                              Model::kOriginal};
  return v;
}

class UnigramModelTest : public test::TestWithParam<Model::EncoderVersion> {
 protected:
  void SetUp() override { encoder_version_ = GetParam(); }
  void TearDown() override {}
  Model::EncoderVersion encoder_version_;
};

void AddPiece(ModelProto *model_proto, const std::string &piece,
              float score = 0.0) {
  auto *sp = model_proto->add_pieces();
  sp->set_piece(piece);
  sp->set_score(score);
}

TEST(UnigramModelTest, SetUnigramModelTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "a");
  AddPiece(&model_proto, "b");
  AddPiece(&model_proto, "c");
  AddPiece(&model_proto, "d");

  const Model model(model_proto);
  EXPECT_EQ(model_proto.SerializeAsString(),
            model.model_proto().SerializeAsString());
}

TEST(UnigramModelTest, SampleEncodeAndScoreTest) {
  // Test whether inclusion probabilities are correct
  ModelProto model_proto = MakeBaseModelProto();
  AddPiece(&model_proto, "A", 0.0);    // 3
  AddPiece(&model_proto, "B", 0.0);    // 4
  AddPiece(&model_proto, "C", 0.1);    // 5
  AddPiece(&model_proto, "AB", 0.2);   // 6
  AddPiece(&model_proto, "BC", 0.5);   // 7
  AddPiece(&model_proto, "ABC", 1.0);  // 8

  Model model(model_proto);

  Lattice lattice;
  lattice.SetSentence("ABC");
  model.PopulateNodes(&lattice);

  std::vector<float> kInv_Theta = {0.0, 1.0};

  for (const auto inv_theta : kInv_Theta) {
    std::vector<std::string> strings = {"ABC", "AB C", "A BC", "A B C"};
    std::map<std::string, float> probs;
    probs["ABC"] = std::exp(inv_theta * 1.0);
    probs["AB C"] = std::exp(inv_theta * (0.2 + 0.1));
    probs["A BC"] = std::exp(inv_theta * (0.0 + 0.5));
    probs["A B C"] = std::exp(inv_theta * (0.0 + 0.0 + 0.1));

    for (const auto& it : strings) {
      EXPECT_EQ(1, probs.count(it));
    }

    double Z = 0.0;
    for (const auto& it : probs) {
      Z += it.second;
    }
    for (auto& it : probs) {
      it.second /= Z;
    }

    std::map<std::pair<std::string, std::string>, float> pair_probs;
    for (const auto& first : strings) {
      for (const auto& second : strings) {
        if (first == second) {
          pair_probs[std::make_pair(first, second)] = 0;
        } else {
          const float first_prob = probs[first];
          const float second_prob = probs[second] / (1 - first_prob);
          pair_probs[std::make_pair(first, second)] = first_prob * second_prob;
        }
      }
    }

    std::map<std::string, float> inclusion_probs;
    for (const auto& string : strings) {
      float inclusion_prob = 0.0;
      for (const auto& other_string : strings) {
        inclusion_prob += pair_probs[std::make_pair(string, other_string)];
      }
      for (const auto& other_string : strings) {
        inclusion_prob += pair_probs[std::make_pair(other_string, string)];
      }
      inclusion_probs[string] = inclusion_prob / 2;
    }
    std::vector<int> kNumSamples = {1, 2};

    for (const auto num_samples : kNumSamples) {
      std::map<std::string, int> counts;
      std::map<std::string, float> scores;
      int kTrials = 50000;
      for (int i = 0; i < kTrials; i++) {
        NBestEncodeResult sample = model.SampleEncodeAndScore(
            "ABC", inv_theta, num_samples, true, false);

        for (const auto& it : sample) {
          std::vector<std::string> tokens;
          for (const auto& inner_it : it.first) {
            tokens.push_back(std::string(inner_it.first));
          }
          std::string sample_string = absl::StrJoin(tokens, " ");
          counts[sample_string] += 1;
          // use the fact that E(1_{i in sample} / score of i) = 1
          // see https://arxiv.org/pdf/1903.06059.pdf appendix D
          scores[sample_string] += std::exp(-it.second);
        }
      }

      // Check that counts and probs are correct
      std::map<std::string, float> probs_to_use =
          (num_samples == 1 ? probs : inclusion_probs);

      for (const auto& it : scores) {
        Z += it.second;
      }
      for (const auto& it : probs_to_use) {
        EXPECT_NEAR(it.second, 1.0 * counts[it.first] / (kTrials * num_samples),
                    0.02);
        // The expectation is quite loose, use a higher tolerance
        EXPECT_NEAR(1.0, scores[it.first] / kTrials, 0.30);
      }
    }
  }
}

TEST_P(UnigramModelTest, PieceToIdTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "a", 0.1);
  AddPiece(&model_proto, "b", 0.2);
  AddPiece(&model_proto, "c", 0.3);
  AddPiece(&model_proto, "d", 0.4);

  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  EXPECT_EQ(model_proto.SerializeAsString(),
            model.model_proto().SerializeAsString());

  EXPECT_NEAR(0.1, model.min_score(), 0.001);
  EXPECT_NEAR(0.4, model.max_score(), 0.001);

  EXPECT_EQ(0, model.PieceToId("<unk>"));
  EXPECT_EQ(1, model.PieceToId("<s>"));
  EXPECT_EQ(2, model.PieceToId("</s>"));
  EXPECT_EQ(3, model.PieceToId("a"));
  EXPECT_EQ(3, model.PieceToId(absl::string_view("a b", 1)));
  EXPECT_EQ(4, model.PieceToId("b"));
  EXPECT_EQ(5, model.PieceToId("c"));
  EXPECT_EQ(6, model.PieceToId("d"));
  EXPECT_EQ(0, model.PieceToId("e"));  // unk
  EXPECT_EQ(0, model.PieceToId(""));   // unk

  EXPECT_EQ("<unk>", model.IdToPiece(0));
  EXPECT_EQ("<s>", model.IdToPiece(1));
  EXPECT_EQ("</s>", model.IdToPiece(2));
  EXPECT_EQ("a", model.IdToPiece(3));
  EXPECT_EQ("b", model.IdToPiece(4));
  EXPECT_EQ("c", model.IdToPiece(5));
  EXPECT_EQ("d", model.IdToPiece(6));

  EXPECT_TRUE(model.IsUnknown(0));
  EXPECT_FALSE(model.IsUnknown(1));
  EXPECT_FALSE(model.IsUnknown(2));
  EXPECT_FALSE(model.IsUnknown(3));
  EXPECT_FALSE(model.IsUnknown(4));
  EXPECT_FALSE(model.IsUnknown(5));
  EXPECT_FALSE(model.IsUnknown(6));

  EXPECT_FALSE(model.IsControl(0));
  EXPECT_TRUE(model.IsControl(1));
  EXPECT_TRUE(model.IsControl(2));
  EXPECT_FALSE(model.IsControl(3));
  EXPECT_FALSE(model.IsControl(4));
  EXPECT_FALSE(model.IsControl(5));
  EXPECT_FALSE(model.IsControl(6));

  EXPECT_NEAR(0, model.GetScore(0), 0.0001);
  EXPECT_NEAR(0, model.GetScore(1), 0.0001);
  EXPECT_NEAR(0, model.GetScore(2), 0.0001);
  EXPECT_NEAR(0.1, model.GetScore(3), 0.0001);
  EXPECT_NEAR(0.2, model.GetScore(4), 0.0001);
  EXPECT_NEAR(0.3, model.GetScore(5), 0.0001);
  EXPECT_NEAR(0.4, model.GetScore(6), 0.0001);

  EXPECT_TRUE(model.Encode("").empty());
}

TEST_P(UnigramModelTest, PopulateNodesAllUnknownsTest) {
  ModelProto model_proto = MakeBaseModelProto();
  AddPiece(&model_proto, "x");
  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  Lattice lattice;
  lattice.SetSentence("abc");
  model.PopulateNodes(&lattice);

  EXPECT_EQ(1, lattice.begin_nodes(0).size());
  EXPECT_EQ(1, lattice.begin_nodes(1).size());
  EXPECT_EQ(1, lattice.begin_nodes(2).size());

  EXPECT_EQ(0, lattice.begin_nodes(0)[0]->id);
  EXPECT_EQ(0, lattice.begin_nodes(1)[0]->id);
  EXPECT_EQ(0, lattice.begin_nodes(2)[0]->id);
}

TEST_P(UnigramModelTest, PopulateNodesTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "a", 0.1);   // 3
  AddPiece(&model_proto, "b", 0.2);   // 4
  AddPiece(&model_proto, "ab", 0.3);  // 5
  AddPiece(&model_proto, "bc", 0.4);  // 6

  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  Lattice lattice;
  lattice.SetSentence("abc");

  model.PopulateNodes(&lattice);

  EXPECT_EQ(2, lattice.begin_nodes(0).size());  // a,ab
  EXPECT_EQ(2, lattice.begin_nodes(1).size());  // b,bc
  EXPECT_EQ(1, lattice.begin_nodes(2).size());  // c(unk)

  EXPECT_EQ(3, lattice.begin_nodes(0)[0]->id);
  EXPECT_EQ(5, lattice.begin_nodes(0)[1]->id);
  EXPECT_EQ(4, lattice.begin_nodes(1)[0]->id);
  EXPECT_EQ(6, lattice.begin_nodes(1)[1]->id);
  EXPECT_EQ(0, lattice.begin_nodes(2)[0]->id);

  EXPECT_NEAR(0.1, lattice.begin_nodes(0)[0]->score, 0.001);
  EXPECT_NEAR(0.3, lattice.begin_nodes(0)[1]->score, 0.001);
  EXPECT_NEAR(0.2, lattice.begin_nodes(1)[0]->score, 0.001);
  EXPECT_NEAR(0.4, lattice.begin_nodes(1)[1]->score, 0.001);
}

TEST_P(UnigramModelTest, PopulateNodesWithUnusedTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "a", 0.1);   // 3
  AddPiece(&model_proto, "b", 0.2);   // 4
  AddPiece(&model_proto, "ab", 0.3);  // 5
  AddPiece(&model_proto, "bc", 0.4);  // 6

  model_proto.mutable_pieces(5)->set_type(ModelProto::SentencePiece::UNUSED);
  model_proto.mutable_pieces(6)->set_type(ModelProto::SentencePiece::UNUSED);

  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  Lattice lattice;
  lattice.SetSentence("abc");

  model.PopulateNodes(&lattice);

  EXPECT_EQ(1, lattice.begin_nodes(0).size());  // a
  EXPECT_EQ(1, lattice.begin_nodes(1).size());  // b
  EXPECT_EQ(1, lattice.begin_nodes(2).size());  // c(unk)
  EXPECT_EQ(3, lattice.begin_nodes(0)[0]->id);
  EXPECT_EQ(4, lattice.begin_nodes(1)[0]->id);
  EXPECT_EQ(0, lattice.begin_nodes(2)[0]->id);
}

TEST_P(UnigramModelTest, ModelNBestTest) {
  ModelProto model_proto = MakeBaseModelProto();
  AddPiece(&model_proto, "a", 0.0);     // 3
  AddPiece(&model_proto, "b", 0.0);     // 4
  AddPiece(&model_proto, "c", 0.0);     // 5
  AddPiece(&model_proto, "ab", 2.0);    // 6
  AddPiece(&model_proto, "bc", 5.0);    // 7
  AddPiece(&model_proto, "abc", 10.0);  // 8

  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  auto nbest = model.NBestEncode("", 10);
  EXPECT_EQ(1, nbest.size());
  EXPECT_TRUE(nbest[0].first.empty());
  EXPECT_EQ(0.0, nbest[0].second);

  nbest = model.NBestEncode("abc", 10);
  EXPECT_EQ(4, nbest.size());

  auto sample = model.SampleEncode("", 0.1);
  EXPECT_EQ(0, sample.size());
  sample = model.SampleEncode("abc", 0.1);
  EXPECT_FALSE(sample.empty());
}

TEST_P(UnigramModelTest, EncodeTest) {
  ModelProto model_proto = MakeBaseModelProto();
  AddPiece(&model_proto, "ab", 0.0);         // 3
  AddPiece(&model_proto, "cd", -0.1);        // 4
  AddPiece(&model_proto, "abc", -0.2);       // 5
  AddPiece(&model_proto, "a", -0.3);         // 6
  AddPiece(&model_proto, "b", -0.4);         // 7
  AddPiece(&model_proto, "c", -0.5);         // 8
  AddPiece(&model_proto, "ABC", -0.5);       // 9
  AddPiece(&model_proto, "abcdabcd", -0.5);  // 10
  AddPiece(&model_proto, "q", -0.5);         // 11
  AddPiece(&model_proto, "r", -0.5);         // 12
  AddPiece(&model_proto, "qr", -0.5);        // 13
  model_proto.mutable_pieces(9)->set_type(   // ABC
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(10)->set_type(  // abcdabcd
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(11)->set_type(  // q
      ModelProto::SentencePiece::USER_DEFINED);
  model_proto.mutable_pieces(12)->set_type(  // r
      ModelProto::SentencePiece::USER_DEFINED);

  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);

  EncodeResult result;

  result = model.Encode("abc");
  EXPECT_EQ(1, result.size());
  EXPECT_EQ("abc", result[0].first);

  result = model.Encode("AB");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("A", result[0].first);
  EXPECT_EQ("B", result[1].first);

  result = model.Encode("abcd");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("cd", result[1].first);

  result = model.Encode("abcc");
  EXPECT_EQ(2, result.size());
  EXPECT_EQ("abc", result[0].first);
  EXPECT_EQ("c", result[1].first);

  result = model.Encode("xabcabaabcdd");
  EXPECT_EQ(7, result.size());
  EXPECT_EQ("x", result[0].first);
  EXPECT_EQ("abc", result[1].first);
  EXPECT_EQ("ab", result[2].first);
  EXPECT_EQ("a", result[3].first);
  EXPECT_EQ("ab", result[4].first);
  EXPECT_EQ("cd", result[5].first);
  EXPECT_EQ("d", result[6].first);

  // all unknown.
  result = model.Encode("xyz東京");
  EXPECT_EQ(5, result.size());
  EXPECT_EQ("x", result[0].first);
  EXPECT_EQ("y", result[1].first);
  EXPECT_EQ("z", result[2].first);
  EXPECT_EQ("東", result[3].first);
  EXPECT_EQ("京", result[4].first);

  // User defined
  result = model.Encode("ABC");
  EXPECT_EQ(1, result.size());
  EXPECT_EQ("ABC", result[0].first);

  result = model.Encode("abABCcd");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("ABC", result[1].first);
  EXPECT_EQ("cd", result[2].first);

  // middle "abcdabcd" is user defined.
  result = model.Encode("ababcdabcdcd");
  EXPECT_EQ(3, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("abcdabcd", result[1].first);
  EXPECT_EQ("cd", result[2].first);

  result = model.Encode("abqrcd");
  EXPECT_EQ(4, result.size());
  EXPECT_EQ("ab", result[0].first);
  EXPECT_EQ("q", result[1].first);
  EXPECT_EQ("r", result[2].first);
  EXPECT_EQ("cd", result[3].first);
}

TEST_P(UnigramModelTest, EncodeWithUnusedTest) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "abcd", 10.0);  // 3
  AddPiece(&model_proto, "abc", 5.0);    // 4
  AddPiece(&model_proto, "ab", 2.0);     // 5
  AddPiece(&model_proto, "cd", 1.0);     // 6
  AddPiece(&model_proto, "a", 0.0);      // 7
  AddPiece(&model_proto, "b", 0.0);      // 8
  AddPiece(&model_proto, "c", 0.0);      // 9
  AddPiece(&model_proto, "d", 0.0);      // 10

  // No unused.
  {
    Model model(model_proto);
    model.SetEncoderVersion(encoder_version_);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(1, result.size());
    EXPECT_EQ("abcd", result[0].first);
  }

  {
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    Model model(model_proto);
    model.SetEncoderVersion(encoder_version_);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("abc", result[0].first);
    EXPECT_EQ("d", result[1].first);
  }

  {
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(5)->set_type(ModelProto::SentencePiece::UNUSED);
    Model model(model_proto);
    model.SetEncoderVersion(encoder_version_);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("abc", result[0].first);
    EXPECT_EQ("d", result[1].first);
  }

  {
    // This is different from BPE segmentation.
    // Unigram language model simply finds the best path without unused nodes.
    model_proto.mutable_pieces(3)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(4)->set_type(ModelProto::SentencePiece::UNUSED);
    model_proto.mutable_pieces(5)->set_type(ModelProto::SentencePiece::NORMAL);
    Model model(model_proto);
    model.SetEncoderVersion(encoder_version_);
    const auto result = model.Encode("abcd");
    EXPECT_EQ(2, result.size());
    EXPECT_EQ("ab", result[0].first);
    EXPECT_EQ("cd", result[1].first);
  }
}

TEST_P(UnigramModelTest, VerifyOutputsEquivalent) {
  ModelProto model_proto = MakeBaseModelProto();

  AddPiece(&model_proto, "abcd", 10.0);  // 3
  AddPiece(&model_proto, "abc", 5.0);    // 4
  AddPiece(&model_proto, "ab", 6.0);     // 5
  AddPiece(&model_proto, "cd", 4.0);     // 6
  AddPiece(&model_proto, "a", 4.0);      // 7
  AddPiece(&model_proto, "b", 1.9);      // 8
  AddPiece(&model_proto, "c", 2.0);      // 9
  AddPiece(&model_proto, "d", 1.0);      // 10
  Model model(model_proto);
  model.SetEncoderVersion(encoder_version_);
  // Equivalent outputs.
  EXPECT_TRUE(model.VerifyOutputsEquivalent("", ""));
  EXPECT_TRUE(model.VerifyOutputsEquivalent("a b", "a b"));
  EXPECT_TRUE(model.VerifyOutputsEquivalent("abcd", "ab cd"));

  // Inequivalent outputs.
  EXPECT_FALSE(model.VerifyOutputsEquivalent("a", "a b"));
  EXPECT_FALSE(model.VerifyOutputsEquivalent("ab", "a b"));
}

INSTANTIATE_TEST_SUITE_P(ParametrizedUnigramModelTests,
                         UnigramModelTest,
                         test::ValuesIn(GetEncoderVersions()));

}  // namespace unigram
}  // namespace sentencepiece
