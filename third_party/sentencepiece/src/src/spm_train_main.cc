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

#include <map>

#include <gflags/gflags.h>

#include "absl/strings/ascii.h"
#include "absl/strings/str_split.h"
#include "src/sentencepiece_model.pb.h"
#include "src/sentencepiece_trainer.h"
#include "src/util.h"

using sentencepiece::NormalizerSpec;
using sentencepiece::TrainerSpec;

namespace {
static sentencepiece::TrainerSpec kDefaultTrainerSpec;
static sentencepiece::NormalizerSpec kDefaultNormalizerSpec;
}  // namespace

DEFINE_string(input, "", "comma separated list of input sentences");
DEFINE_string(input_format, kDefaultTrainerSpec.input_format(),
              "Input format. Supported format is `text` or `tsv`.");
DEFINE_string(model_prefix, "", "output model prefix");
DEFINE_string(model_type, "unigram",
              "model algorithm: unigram, bpe, word or char");
DEFINE_int32(vocab_size, kDefaultTrainerSpec.vocab_size(), "vocabulary size");
DEFINE_string(accept_language, "",
              "comma-separated list of languages this model can accept");
DEFINE_int32(self_test_sample_size, kDefaultTrainerSpec.self_test_sample_size(),
             "the size of self test samples");
DEFINE_double(character_coverage, kDefaultTrainerSpec.character_coverage(),
              "character coverage to determine the minimum symbols");
DEFINE_int32(input_sentence_size, kDefaultTrainerSpec.input_sentence_size(),
             "maximum size of sentences the trainer loads");
DEFINE_bool(shuffle_input_sentence,
            kDefaultTrainerSpec.shuffle_input_sentence(),
            "Randomly sample input sentences in advance. Valid when "
            "--input_sentence_size > 0");
DEFINE_int32(seed_sentencepiece_size,
             kDefaultTrainerSpec.seed_sentencepiece_size(),
             "the size of seed sentencepieces");
DEFINE_double(shrinking_factor, kDefaultTrainerSpec.shrinking_factor(),
              "Keeps top shrinking_factor pieces with respect to the loss");
DEFINE_int32(num_threads, kDefaultTrainerSpec.num_threads(),
             "number of threads for training");
DEFINE_int32(num_sub_iterations, kDefaultTrainerSpec.num_sub_iterations(),
             "number of EM sub-iterations");
DEFINE_int32(max_sentencepiece_length,
             kDefaultTrainerSpec.max_sentencepiece_length(),
             "maximum length of sentence piece");
DEFINE_int32(max_sentence_length, kDefaultTrainerSpec.max_sentence_length(),
             "maximum length of sentence in byte");
DEFINE_bool(split_by_unicode_script,
            kDefaultTrainerSpec.split_by_unicode_script(),
            "use Unicode script to split sentence pieces");
DEFINE_bool(split_by_number, kDefaultTrainerSpec.split_by_number(),
            "split tokens by numbers (0-9)");
DEFINE_bool(split_by_whitespace, kDefaultTrainerSpec.split_by_whitespace(),
            "use a white space to split sentence pieces");
DEFINE_bool(treat_whitespace_as_suffix,
            kDefaultTrainerSpec.treat_whitespace_as_suffix(),
            "treat whitespace marker as suffix instead of prefix.");
DEFINE_string(control_symbols, "", "comma separated list of control symbols");
DEFINE_string(user_defined_symbols, "",
              "comma separated list of user defined symbols");
DEFINE_bool(vocabulary_output_piece_score,
            kDefaultTrainerSpec.vocabulary_output_piece_score(),
            "Define score in vocab file");
DEFINE_string(normalization_rule_name, "nmt_nfkc",
              "Normalization rule name. "
              "Choose from nfkc or identity");
DEFINE_string(normalization_rule_tsv, "", "Normalization rule TSV file. ");
DEFINE_bool(add_dummy_prefix, kDefaultNormalizerSpec.add_dummy_prefix(),
            "Add dummy whitespace at the beginning of text");
DEFINE_bool(remove_extra_whitespaces,
            kDefaultNormalizerSpec.remove_extra_whitespaces(),
            "Removes leading, trailing, and "
            "duplicate internal whitespace");
DEFINE_bool(hard_vocab_limit, kDefaultTrainerSpec.hard_vocab_limit(),
            "If set to false, --vocab_size is considered as a soft limit.");
DEFINE_bool(use_all_vocab, kDefaultTrainerSpec.use_all_vocab(),
            "If set to true, use all tokens as vocab. "
            "Valid for word/char models.");
DEFINE_int32(unk_id, kDefaultTrainerSpec.unk_id(), "Override UNK (<unk>) id.");
DEFINE_int32(bos_id, kDefaultTrainerSpec.bos_id(),
             "Override BOS (<s>) id. Set -1 to disable BOS.");
DEFINE_int32(eos_id, kDefaultTrainerSpec.eos_id(),
             "Override EOS (</s>) id. Set -1 to disable EOS.");
DEFINE_int32(pad_id, kDefaultTrainerSpec.pad_id(),
             "Override PAD (<pad>) id. Set -1 to disable PAD.");
DEFINE_string(unk_piece, kDefaultTrainerSpec.unk_piece(),
              "Override UNK (<unk>) piece.");
DEFINE_string(bos_piece, kDefaultTrainerSpec.bos_piece(),
              "Override BOS (<s>) piece.");
DEFINE_string(eos_piece, kDefaultTrainerSpec.eos_piece(),
              "Override EOS (</s>) piece.");
DEFINE_string(pad_piece, kDefaultTrainerSpec.pad_piece(),
              "Override PAD (<pad>) piece.");
DEFINE_string(unk_surface, kDefaultTrainerSpec.unk_surface(),
              "Dummy surface string for <unk>. In decoding <unk> is decoded to "
              "`unk_surface`.");


int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  sentencepiece::TrainerSpec trainer_spec;
  sentencepiece::NormalizerSpec normalizer_spec;

  CHECK(!FLAGS_input.empty());
  CHECK(!FLAGS_model_prefix.empty());

// Populates the value from flags to spec.
#define SetTrainerSpecFromFlag(name) trainer_spec.set_##name(FLAGS_##name);

#define SetNormalizerSpecFromFlag(name) \
  normalizer_spec.set_##name(FLAGS_##name);

#define SetRepeatedTrainerSpecFromFlag(name)                                   \
  if (!FLAGS_##name.empty()) {                                                 \
    const std::vector<std::string> values = absl::StrSplit(FLAGS_##name, ','); \
    for (const auto v : values) {                                              \
      trainer_spec.add_##name(v);                                              \
    }                                                                          \
  }


  SetTrainerSpecFromFlag(input_format);
  SetTrainerSpecFromFlag(model_prefix);
  SetTrainerSpecFromFlag(vocab_size);
  SetTrainerSpecFromFlag(self_test_sample_size);
  SetTrainerSpecFromFlag(character_coverage);
  SetTrainerSpecFromFlag(input_sentence_size);
  SetTrainerSpecFromFlag(shuffle_input_sentence);
  SetTrainerSpecFromFlag(seed_sentencepiece_size);
  SetTrainerSpecFromFlag(shrinking_factor);
  SetTrainerSpecFromFlag(num_threads);
  SetTrainerSpecFromFlag(num_sub_iterations);
  SetTrainerSpecFromFlag(max_sentencepiece_length);
  SetTrainerSpecFromFlag(max_sentence_length);
  SetTrainerSpecFromFlag(split_by_unicode_script);
  SetTrainerSpecFromFlag(split_by_whitespace);
  SetTrainerSpecFromFlag(split_by_number);
  SetTrainerSpecFromFlag(treat_whitespace_as_suffix);
  SetTrainerSpecFromFlag(hard_vocab_limit);
  SetTrainerSpecFromFlag(use_all_vocab);
  SetTrainerSpecFromFlag(unk_id);
  SetTrainerSpecFromFlag(bos_id);
  SetTrainerSpecFromFlag(eos_id);
  SetTrainerSpecFromFlag(pad_id);
  SetTrainerSpecFromFlag(unk_piece);
  SetTrainerSpecFromFlag(bos_piece);
  SetTrainerSpecFromFlag(eos_piece);
  SetTrainerSpecFromFlag(pad_piece);
  SetTrainerSpecFromFlag(unk_surface);
  SetRepeatedTrainerSpecFromFlag(accept_language);
  SetRepeatedTrainerSpecFromFlag(control_symbols);
  SetRepeatedTrainerSpecFromFlag(user_defined_symbols);

  normalizer_spec.set_name(FLAGS_normalization_rule_name);
  SetNormalizerSpecFromFlag(normalization_rule_tsv);
  SetNormalizerSpecFromFlag(add_dummy_prefix);
  SetNormalizerSpecFromFlag(remove_extra_whitespaces);

  CHECK_OK(sentencepiece::SentencePieceTrainer::PopulateModelTypeFromString(
      FLAGS_model_type, &trainer_spec));


  CHECK_OK(sentencepiece::SentencePieceTrainer::Train(trainer_spec,
                                                      normalizer_spec));

  return 0;
}
