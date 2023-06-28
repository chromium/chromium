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

#include "src/trainer_interface.h"

#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "src/filesystem.h"
#include "src/model_factory.h"
#include "src/model_interface.h"
#include "src/normalizer.h"
#include "src/sentencepiece_processor.h"
#include "src/sentencepiece_trainer.h"
#include "src/unicode_script.h"
#include "src/util.h"
#include "src/deps/threadpool.h"

namespace sentencepiece {

const char32 TrainerInterface::kWSChar = L'\u2581';
const char TrainerInterface::kWSStr[] = "\xe2\x96\x81";

const char32 TrainerInterface::kUNKChar = L'\u2585';
const char TrainerInterface::kUNKStr[] = "\xe2\x96\x85";

const char32 TrainerInterface::kUPPBoundaryChar = L'\u0009';
const char TrainerInterface::kUPPBoundaryStr[] = "\t";

namespace {
::util::Status VerifySpec(const TrainerSpec &trainer_spec) {
  CHECK_OR_RETURN(!trainer_spec.model_prefix().empty());
  CHECK_GT_OR_RETURN(trainer_spec.input().size(), 0);
  CHECK_GT_OR_RETURN(trainer_spec.vocab_size(), 0);

  if (trainer_spec.model_type() == TrainerSpec::UNIGRAM ||
      trainer_spec.model_type() == TrainerSpec::BPE) {
    CHECK_OR_RETURN(!trainer_spec.use_all_vocab())
        << "--use_all_vocab=true is valid for WORD/CHAR model.";
  }

  if (trainer_spec.has_mining_sentence_size()) {
    LOG(WARNING)
        << "--mining_sentence_size() is deprecated. Use --input_sentence_size";
  }

  if (trainer_spec.has_training_sentence_size()) {
    LOG(WARNING) << "--training_sentence_size() is deprecated. Use "
                    "--input_sentence_size";
  }

#define CHECK_RANGE(variable, minval, maxval) \
  CHECK_OR_RETURN(variable >= minval && variable <= maxval)

  CHECK_RANGE(trainer_spec.character_coverage(), 0.98, 1.0);
  CHECK_RANGE(trainer_spec.max_sentencepiece_length(), 1, 512);
  CHECK_RANGE(trainer_spec.num_sub_iterations(), 1, 10);
  CHECK_RANGE(trainer_spec.num_threads(), 1, 128);
  CHECK_RANGE(trainer_spec.self_test_sample_size(), 0, 1000);
  CHECK_RANGE(trainer_spec.shrinking_factor(), 0.5, 0.95);
  CHECK_RANGE(trainer_spec.max_sentence_length(), 10, 1073741824);
#undef CHECK_RANGE

  CHECK_OR_RETURN(trainer_spec.input_sentence_size() <= 0 ||
                  trainer_spec.input_sentence_size() > 100);

  CHECK_OR_RETURN(!trainer_spec.unk_piece().empty());
  CHECK_OR_RETURN(!trainer_spec.bos_piece().empty());
  CHECK_OR_RETURN(!trainer_spec.eos_piece().empty());
  CHECK_OR_RETURN(!trainer_spec.pad_piece().empty());

  if (SentencePieceTrainer::GetPretokenizerForTraining()) {
    CHECK_EQ_OR_RETURN(TrainerSpec::UNIGRAM, trainer_spec.model_type())
        << "PretokenizerForTraining is only supported in UNIGRAM mode.";
  }

  return ::util::OkStatus();
}

class SentenceSelector {
 public:
  using Sampler = random::ReservoirSampler<TrainerInterface::Sentence>;

  static constexpr int64 kTooBigSentencesSize = 1000000;

  SentenceSelector(TrainerInterface::Sentences *sentences,
                   const TrainerSpec &spec)
      : sentences_(sentences), spec_(&spec) {
    if (spec_->input_sentence_size() > 0) {
      if (spec_->shuffle_input_sentence()) {
        constexpr size_t kSeed = 12345678;
        sampler_ = absl::make_unique<Sampler>(
            sentences, spec_->input_sentence_size(), kSeed);
      } else {
        LOG(INFO)
            << "First " << spec_->input_sentence_size()
            << " sentences are selected. Remaining sentences are discarded.";
      }
    }
  }

  void Finish() const {
    if (sentences_->size() > kTooBigSentencesSize) {
      LOG(WARNING) << "Too many sentences are loaded! (" << sentences_->size()
                   << "), which may slow down training.";
      LOG(WARNING) << "Consider using "
                      "--input_sentence_size=<size> and "
                      "--shuffle_input_sentence=true.";
      LOG(WARNING) << "They allow to randomly sample <size> sentences from "
                      "the entire corpus.";
    }
  }

  bool Add(const std::pair<std::string, int64> &sentence) {
    if (spec_->input_sentence_size() <= 0) {
      sentences_->emplace_back(sentence);
    } else {
      if (spec_->shuffle_input_sentence()) {
        sampler_->Add(sentence);
      } else {
        sentences_->emplace_back(sentence);
        if (sentences_->size() >=
            static_cast<size_t>(spec_->input_sentence_size()))
          return false;
      }
    }

    if (total_size() > 0 && total_size() % kTooBigSentencesSize == 0) {
      LOG(INFO) << "Loaded " << total_size() << " lines";
    }

    return true;
  }

  size_t total_size() const {
    return sampler_.get() ? sampler_->total_size() : sentences_->size();
  }

 private:
  TrainerInterface::Sentences *sentences_ = nullptr;
  const TrainerSpec *spec_ = nullptr;
  std::unique_ptr<Sampler> sampler_;
};
}  // namespace

TrainerInterface::TrainerInterface(const TrainerSpec &trainer_spec,
                                   const NormalizerSpec &normalizer_spec)
    : trainer_spec_(trainer_spec), normalizer_spec_(normalizer_spec) {
  status_ = VerifySpec(trainer_spec_);
  if (status_.ok()) status_ = InitMetaPieces();
}

TrainerInterface::~TrainerInterface() {}

bool TrainerInterface::IsValidSentencePiece(
    const string_util::UnicodeText &sentencepiece) const {
  // Returns false if the length of piece is invalid.
  if (sentencepiece.empty() ||
      sentencepiece.size() >
          static_cast<size_t>(trainer_spec_.max_sentencepiece_length())) {
    return false;
  }

  constexpr unicode_script::ScriptType kAnyType =
      static_cast<unicode_script::ScriptType>(-1);

  auto is_number = [](char32 c) { return (c >= 0x30 && c <= 0x39); };

  unicode_script::ScriptType prev_script = kAnyType;

  for (size_t pos = 0; pos < sentencepiece.size(); ++pos) {
    const char32 c = sentencepiece[pos];
    if (c == kUNKChar) {  // UNK must not be included
      return false;
    }
    if (c == 0x0000) {  // NULL is not allowed for Darts (TRIE).
      return false;
    }
    // kUPPBoundaryChar is included when split_by_upp_for_training is true.
    if (c == kUPPBoundaryChar) {
      return false;
    }
    if (c == 0x0020) {
      LOG(WARNING) << "space must not be included in normalized string.";
      return false;
    }
    if (!string_util::IsValidCodepoint(c)) {
      return false;
    }

    if (c == kWSChar) {
      // Only allows whitespace to appear as a prefix of piece.
      // When split_by_whitespace is false, we allow whitespaces to
      // appear in the middle, "foo_bar", but do not allow them
      // to appear as suffix, "foo_bar_".
      // Regardless of the setting of split_by_whitespace,
      // whitespace is treated as a prefix/infix of symbol or
      // independent symbol.
      if (trainer_spec_.treat_whitespace_as_suffix()) {
        if ((trainer_spec_.split_by_whitespace() &&
             pos < sentencepiece.size() - 1) ||
            (!trainer_spec_.split_by_whitespace() &&
             pos < sentencepiece.size() - 1 && pos == 0)) {
          return false;
        }
      } else {
        if ((trainer_spec_.split_by_whitespace() && pos > 0) ||
            (!trainer_spec_.split_by_whitespace() && pos > 0 &&
             pos == sentencepiece.size() - 1)) {
          return false;
        }
      }
    } else {
      auto s = unicode_script::GetScript(c);

      // Merge Hiragana/Katakana into Han.
      if (s == unicode_script::U_Hiragana || s == unicode_script::U_Katakana ||
          c == 0x30FC) {  // long vowel sound (Katakana) should be Katakana
        s = unicode_script::U_Han;
      }

      if (!trainer_spec_.split_by_number() && is_number(c)) {
        s = kAnyType;
      }

      // Do not allow a piece to include multiple Unicode scripts
      // when split_by_unicode_script() is true (default = true).
      if (trainer_spec_.split_by_unicode_script() && s != kAnyType &&
          prev_script != kAnyType && prev_script != s) {
        return false;
      }

      prev_script = s;
    }
  }
  return true;
}

::util::Status TrainerInterface::LoadSentences() {
  RETURN_IF_ERROR(status());
  CHECK_OR_RETURN(sentences_.empty());
  CHECK_OR_RETURN(required_chars_.empty());
  CHECK_OR_RETURN(trainer_spec_.input_format().empty() ||
                  trainer_spec_.input_format() == "text" ||
                  trainer_spec_.input_format() == "tsv")
      << "Supported formats are 'text' and 'tsv'.";

  const bool is_tsv = trainer_spec_.input_format() == "tsv";

  SentenceSelector selector(&sentences_, trainer_spec_);
  random::ReservoirSampler<std::string> test_sentence_sampler(
      &self_test_samples_, trainer_spec_.self_test_sample_size());

  int too_long_lines = 0;

  for (const auto &filename : trainer_spec_.input()) {
    LOG(INFO) << "Loading corpus: " << filename;
    std::string sentence;
    auto input = filesystem::NewReadableFile(filename);
    RETURN_IF_ERROR(input->status());
    while (input->ReadLine(&sentence)) {
      int64 freq = 1;
      if (is_tsv) {
        const std::vector<std::string> v = absl::StrSplit(sentence, "\t");
        CHECK_EQ_OR_RETURN(v.size(), 2)
            << "Input format must be: word <tab> freq. " << sentence;
        sentence = v[0];
        CHECK_OR_RETURN(absl::SimpleAtoi(v[1], &freq))
            << "Could not parse the frequency";
        CHECK_GE_OR_RETURN(freq, 1);
      }

      if (sentence.empty()) continue;

      if (static_cast<int>(sentence.size()) >
          trainer_spec_.max_sentence_length()) {
        if (too_long_lines == 0) {
          LOG(WARNING) << "Found too long line (" << sentence.size() << " > "
                       << trainer_spec_.max_sentence_length() << ").";
          LOG(WARNING) << "Too long lines are skipped in the training.";
          LOG(WARNING) << "The maximum length can be changed with "
                          "--max_sentence_length=<size> flag.";
        }
        ++too_long_lines;
        continue;
      }

      if (sentence.find(kUNKStr) != std::string::npos) {
        LOG(INFO) << "Reserved chars are found. Skipped: " << sentence;
        continue;
      }

      test_sentence_sampler.Add(sentence);

      if (!selector.Add(std::make_pair(sentence, freq))) {
        goto END;
      }
    }
  }

END:
  // Emits error message if any.
  selector.Finish();

  if (sentences_.size() == selector.total_size()) {
    LOG(INFO) << "Loaded all " << sentences_.size() << " sentences";
  } else {
    LOG(INFO) << "Sampled " << sentences_.size() << " sentences from "
              << selector.total_size() << " sentences.";
  }
  if (too_long_lines > 0)
    LOG(INFO) << "Skipped " << too_long_lines << " too long sentences.";
  if (self_test_samples_.size() > 0)
    LOG(INFO) << "Loaded " << self_test_samples_.size() << " test sentences";

  // Normalize and removes empty string.
  {
    const normalizer::Normalizer normalizer(normalizer_spec_);
    std::set<absl::string_view> meta_pieces_set;
    for (const auto &it : meta_pieces_) {
      LOG(INFO) << "Adding meta_piece: " << it.second.first;
      meta_pieces_set.insert(it.second.first);
    }
    const normalizer::PrefixMatcher meta_pieces_matcher(meta_pieces_set);

    LOG(INFO) << "Normalizing sentences...";
    CHECK_OR_RETURN(!sentences_.empty());
    {
      auto pool = absl::make_unique<ThreadPool>(trainer_spec_.num_threads());
      pool->StartWorkers();
      for (int n = 0; n < trainer_spec_.num_threads(); ++n) {
        pool->Schedule([&, n]() {
          for (size_t i = n; i < sentences_.size();
               i += trainer_spec_.num_threads()) {
            auto *s = &sentences_[i].first;
            *s = meta_pieces_matcher.GlobalReplace(normalizer.Normalize(*s),
                                                   kUPPBoundaryStr);
          }
        });
      }
    }

    for (size_t i = 0; i < sentences_.size(); ++i) {
      auto *s = &sentences_[i].first;
      CHECK_OR_RETURN(s->find(" ") == std::string::npos)
          << "Normalized string must not include spaces";
      if (s->empty()) {
        std::swap(sentences_[i], sentences_[sentences_.size() - 1]);
        sentences_.resize(sentences_.size() - 1);
      }
    }
  }

  // Count character frequencies.
  int64 all_chars_count = 0;
  absl::flat_hash_map<char32, int64> chars_count;
  for (const auto &w : sentences_) {
    for (const char32 c : string_util::UTF8ToUnicodeText(w.first)) {
      if (!string_util::IsValidCodepoint(c)) continue;
      if (c == 0x0000) {
        LOG(INFO)
            << "Found null character. The corpus must be encoded in utf-8.";
        continue;
      }
      if (c == 0x0020) {
        // UTF8ToUnicodeText returns a white space if the text
        // contains an interchange-invalid character.
        CHECK_OR_RETURN(w.first.find(" ") == std::string::npos)
            << "space must not be included in normalized string.";
        continue;
      }
      chars_count[c] += w.second;
      all_chars_count += w.second;
    }
  }
  LOG(INFO) << "all chars count=" << all_chars_count;

  // Determines required_chars which must be included in the vocabulary.
  int64 accumulated_chars_count = 0;
  for (const auto &w : Sorted(chars_count)) {
    const float coverage = 1.0 * accumulated_chars_count / all_chars_count;
    if (!trainer_spec_.use_all_vocab() &&
        coverage >= trainer_spec_.character_coverage()) {
      LOG(INFO) << "Done: " << 100.0 * coverage << "% characters are covered.";
      break;
    }
    accumulated_chars_count += w.second;
    CHECK_NE_OR_RETURN(w.first, 0x0020)
        << "space must not be included in normalized string.";
    if (w.first == kUPPBoundaryChar) continue;  // Tab is not included.
    required_chars_.insert(w);
  }

  LOG(INFO) << "Alphabet size=" << required_chars_.size();
  LOG(INFO) << "Final character coverage="
            << 1.0 * accumulated_chars_count / all_chars_count;

  CHECK_OR_RETURN(!port::ContainsKey(required_chars_, kUNKChar));

  // Replaces rare characters (characters not included in required_chars_)
  // with kUNKChar.
  for (auto &w : sentences_) {
    string_util::UnicodeText uw2;
    for (const char32 c : string_util::UTF8ToUnicodeText(w.first)) {
      if (port::ContainsKey(required_chars_, c)) {
        uw2.push_back(c);
      } else {
        uw2.push_back(kUNKChar);
      }
    }
    w.first = string_util::UnicodeTextToUTF8(uw2);
  }

  // +3 for meta pieces.
  if (trainer_spec_.model_type() != TrainerSpec::WORD &&
      trainer_spec_.model_type() != TrainerSpec::CHAR) {
    CHECK_LE_OR_RETURN(
        static_cast<int>(required_chars_.size() + meta_pieces_.size()),
        trainer_spec_.vocab_size())
        << "Vocabulary size is smaller than required_chars. "
        << trainer_spec_.vocab_size() << " vs "
        << required_chars_.size() + meta_pieces_.size() << ". "
        << "Increase vocab_size or decrease character_coverage with "
        << "--character_coverage option.";
  }

  LOG(INFO) << "Done! preprocessed " << sentences_.size() << " sentences.";

  return ::util::OkStatus();
}

void TrainerInterface::SplitSentencesByWhitespace() {
  LOG(INFO) << "Tokenizing input sentences with whitespace: "
            << sentences_.size();
  absl::flat_hash_map<std::string, int64> tokens;
  for (const auto &s : sentences_) {
    for (const auto &w :
         SplitIntoWords(s.first, trainer_spec_.treat_whitespace_as_suffix())) {
      tokens[std::string(w)] += s.second;
    }
  }
  sentences_ = Sorted(tokens);
  LOG(INFO) << "Done! " << sentences_.size();
}

::util::Status TrainerInterface::Serialize(ModelProto *model_proto) const {
  RETURN_IF_ERROR(status());

  // Duplicated sentencepiece is not allowed.
  std::set<std::string> dup;

#define CHECK_PIECE(piece)                                  \
  CHECK_OR_RETURN(string_util::IsStructurallyValid(piece)); \
  CHECK_OR_RETURN(!piece.empty());                          \
  CHECK_OR_RETURN(dup.insert(piece).second) << piece << " is already defined";

  size_t fid = 0;
  for (int id = 0; id < trainer_spec_.vocab_size(); ++id) {
    const auto it = meta_pieces_.find(id);
    if (it != meta_pieces_.end()) {
      auto *sp = model_proto->add_pieces();
      sp->set_piece(it->second.first);
      sp->set_type(it->second.second);
      sp->set_score(0.0);
      CHECK_EQ_OR_RETURN(model_proto->pieces_size() - 1, it->first);
      CHECK_NE_OR_RETURN(ModelProto::SentencePiece::NORMAL, sp->type());
      CHECK_PIECE(sp->piece());
    } else if (fid < final_pieces_.size()) {
      const auto &w = final_pieces_[fid++];
      auto *sp = model_proto->add_pieces();
      sp->set_piece(w.first);
      sp->set_score(w.second);
      CHECK_PIECE(sp->piece());
    }
  }

  CHECK_EQ_OR_RETURN(fid, final_pieces_.size());

  *(model_proto->mutable_trainer_spec()) = trainer_spec_;
  *(model_proto->mutable_normalizer_spec()) = normalizer_spec_;

  if (!trainer_spec_.hard_vocab_limit() ||
      trainer_spec_.model_type() == TrainerSpec::CHAR) {
    CHECK_GE_OR_RETURN(trainer_spec_.vocab_size(), model_proto->pieces_size());
    CHECK_GE_OR_RETURN(trainer_spec_.vocab_size(),
                       static_cast<int32>(dup.size()));
    model_proto->mutable_trainer_spec()->set_vocab_size(
        model_proto->pieces_size());
  } else {
    CHECK_EQ_OR_RETURN(trainer_spec_.vocab_size(), model_proto->pieces_size());
    CHECK_EQ_OR_RETURN(trainer_spec_.vocab_size(),
                       static_cast<int32>(dup.size()));
  }

  return ::util::OkStatus();
}

::util::Status TrainerInterface::SaveModel(absl::string_view filename) const {
  LOG(INFO) << "Saving model: " << filename;
  ModelProto model_proto;
  RETURN_IF_ERROR(Serialize(&model_proto));

  // Saves self-testing data.
  if (!self_test_samples_.empty()) {
    SentencePieceProcessor sp;
    RETURN_IF_ERROR(sp.Load(model_proto));
    for (const auto &input : self_test_samples_) {
      std::vector<std::string> sps;
      RETURN_IF_ERROR(sp.Encode(input, &sps));
      auto *sample = model_proto.mutable_self_test_data()->add_samples();
      sample->set_input(input);
      sample->set_expected(absl::StrJoin(sps, " "));
    }
  }

  auto output = filesystem::NewWritableFile(filename.data(), true);
  RETURN_IF_ERROR(output->status());
  output->Write(model_proto.SerializeAsString());
  return ::util::OkStatus();
}

::util::Status TrainerInterface::SaveVocab(absl::string_view filename) const {
  LOG(INFO) << "Saving vocabs: " << filename;
  ModelProto model_proto;
  RETURN_IF_ERROR(Serialize(&model_proto));
  auto output = filesystem::NewWritableFile(filename);
  RETURN_IF_ERROR(output->status());

  if (trainer_spec_.vocabulary_output_piece_score()) {
    for (const auto &piece : model_proto.pieces()) {
      std::ostringstream os;
      os << piece.piece() << "\t" << piece.score();
      CHECK_OR_RETURN(output->WriteLine(os.str()));
    }
  } else {
    for (const auto &piece : model_proto.pieces()) {
      CHECK_OR_RETURN(output->WriteLine(piece.piece()));
    }
  }

  return ::util::OkStatus();
}

::util::Status TrainerInterface::Save() const {
  RETURN_IF_ERROR(SaveModel(trainer_spec_.model_prefix() + ".model"));
  RETURN_IF_ERROR(SaveVocab(trainer_spec_.model_prefix() + ".vocab"));
  return ::util::OkStatus();
}

::util::Status TrainerInterface::InitMetaPieces() {
  CHECK_OR_RETURN(meta_pieces_.empty());
  bool has_unk = false;

  auto insert_id = [&has_unk, this](int id, const std::string &w) -> bool {
    if (id < 0) return true;
    if (id >= trainer_spec_.vocab_size() ||
        meta_pieces_.find(id) != meta_pieces_.end() ||
        (has_unk && w == trainer_spec_.unk_piece()))
      return false;
    if (w == trainer_spec_.unk_piece()) has_unk = true;
    meta_pieces_[id] = std::make_pair(
        w, w == trainer_spec_.unk_piece() ? ModelProto::SentencePiece::UNKNOWN
                                          : ModelProto::SentencePiece::CONTROL);
    return true;
  };

  CHECK_OR_RETURN(insert_id(trainer_spec_.unk_id(), trainer_spec_.unk_piece()));
  CHECK_OR_RETURN(insert_id(trainer_spec_.bos_id(), trainer_spec_.bos_piece()));
  CHECK_OR_RETURN(insert_id(trainer_spec_.eos_id(), trainer_spec_.eos_piece()));
  CHECK_OR_RETURN(insert_id(trainer_spec_.pad_id(), trainer_spec_.pad_piece()));

  CHECK_OR_RETURN(has_unk) << trainer_spec_.unk_piece() << " must be defined.";

  std::set<std::string> dup;

  int id = 0;
  auto insert_meta_symbol = [&id, &dup, this](
                                const std::string &w,
                                ModelProto::SentencePiece::Type type) -> bool {
    if (!dup.insert(w).second) {
      LOG(ERROR) << w << " is already defined.";
      return false;
    }

    if (w == trainer_spec_.unk_piece()) {
      LOG(ERROR) << trainer_spec_.unk_piece()
                 << " must not be defined with --control_symbols and "
                    "--user_defined_symbols.";
      return false;
    }

    if (w == trainer_spec_.bos_piece() && trainer_spec_.bos_id() >= 0) {
      meta_pieces_[trainer_spec_.bos_id()].second = type;
    } else if (w == trainer_spec_.eos_piece() && trainer_spec_.eos_id() >= 0) {
      meta_pieces_[trainer_spec_.eos_id()].second = type;
    } else if (w == trainer_spec_.pad_piece() && trainer_spec_.pad_id() >= 0) {
      meta_pieces_[trainer_spec_.pad_id()].second = type;
    } else {
      while (meta_pieces_.find(id) != meta_pieces_.end()) ++id;
      meta_pieces_[id] = std::make_pair(w, type);
    }
    return true;
  };

  for (const auto &w : trainer_spec_.control_symbols()) {
    CHECK_OR_RETURN(insert_meta_symbol(w, ModelProto::SentencePiece::CONTROL));
  }

  for (const auto &w : trainer_spec_.user_defined_symbols()) {
    CHECK_OR_RETURN(
        insert_meta_symbol(w, ModelProto::SentencePiece::USER_DEFINED));
  }

  return ::util::OkStatus();
}

}  // namespace sentencepiece
