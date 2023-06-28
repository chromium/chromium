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

#ifndef TRAINER_INTERFACE_H_
#define TRAINER_INTERFACE_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest_prod.h"
#include "absl/container/flat_hash_map.h"
#include "src/common.h"
#include "src/sentencepiece_model.pb.h"
#include "src/sentencepiece_processor.h"
#include "src/util.h"

namespace sentencepiece {

template <typename K, typename V>
std::vector<std::pair<K, V>> Sorted(const std::vector<std::pair<K, V>> &m) {
  std::vector<std::pair<K, V>> v = m;
  std::sort(v.begin(), v.end(),
            [](const std::pair<K, V> &p1, const std::pair<K, V> &p2) {
              return (p1.second > p2.second ||
                      (p1.second == p2.second && p1.first < p2.first));
            });
  return v;
}

template <typename K, typename V>
std::vector<std::pair<K, V>> Sorted(const absl::flat_hash_map<K, V> &m) {
  std::vector<std::pair<K, V>> v(m.begin(), m.end());
  return Sorted(v);
}

// Base trainer class
class TrainerInterface {
 public:
  using Sentence = std::pair<std::string, int64>;
  using Sentences = std::vector<Sentence>;

  static const char32 kWSChar;
  static const char32 kUNKChar;
  static const char32 kUPPBoundaryChar;
  static const char kWSStr[];
  static const char kUNKStr[];
  static const char kUPPBoundaryStr[];

  TrainerInterface(const TrainerSpec &trainer_spec,
                   const NormalizerSpec &normalizer_spec);

  virtual ~TrainerInterface();

  virtual ::util::Status Train() { return status(); }

  virtual ::util::Status status() const { return status_; }

  FRIEND_TEST(TrainerInterfaceTest, IsValidSentencePieceTest);
  FRIEND_TEST(TrainerInterfaceTest, OverrideSpecialPiecesTest);
  FRIEND_TEST(TrainerInterfaceTest, SerializeTest);

 protected:
  // Returns true if |piece| is valid sentence piece.
  // The result is affected by
  // max_sentencepiece_length, split_by_whiespace, split_by_unicode_script.
  bool IsValidSentencePiece(const string_util::UnicodeText &piece) const;

  // Loads all sentences from spec.input().
  // It loads at most input_sentence_size sentences.
  ::util::Status LoadSentences();

  // Splits all sentencecs by whitespaces and
  // replace the |sentences_| with tokenized string.
  // e.g.,
  //  [ ["hello world ", 1], ["hi world]" ] =>
  //  [ ["hello", 1], ["hi", 1], ["world", 2] ]
  void SplitSentencesByWhitespace();

  // Save model files into spec.model_prefix().
  ::util::Status Save() const;

  // Set of characters which must be included in the final vocab.
  // The value of this map stores the frequency.
  absl::flat_hash_map<char32, int64> required_chars_;

  // Final output pieces
  std::vector<std::pair<std::string, float>> final_pieces_;

  // All sentences.
  Sentences sentences_;

  // Trainer spec.
  TrainerSpec trainer_spec_;

  // Normalizer spec
  NormalizerSpec normalizer_spec_;

  // Reserved control pieces. e.g., <unk>, <s>, </s>.
  // key is vocab id.
  std::map<int, std::pair<std::string, ModelProto::SentencePiece::Type>>
      meta_pieces_;

  // Detect errors on initialization.
  ::util::Status status_;

 private:
  // Serialize final_pieces_ to |model_proto|.
  ::util::Status Serialize(ModelProto *model_proto) const;

  // Saves the best sentence split with the current model for debugging.
  ::util::Status SaveSplits(absl::string_view filename) const;

  // Saves model file.
  ::util::Status SaveModel(absl::string_view filename) const;

  // Saves vocabulary file for NMT.
  ::util::Status SaveVocab(absl::string_view filename) const;

  // Initializes `meta_pieces_` from TrainerSpec.
  ::util::Status InitMetaPieces();

  // Randomly sampled raw sentences for self-testing.
  std::vector<std::string> self_test_samples_;
};
}  // namespace sentencepiece
#endif  // TRAINER_INTERFACE_H_
