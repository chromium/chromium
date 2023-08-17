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
#include "pretokenizer_for_training.h"

#include <string>

#include "absl/strings/str_replace.h"

namespace sentencepiece {
namespace pretokenizer {

namespace {
// TODO(taku): They are defined in trainer_interface.h but we
// defined them explicitly to avoid the dependency to trainier_interface.
// Currently, we have no separated build rules.
const char kWSStr[] = "\xe2\x96\x81";
}  // namespace

std::vector<std::string> PretokenizerForTrainingInterface::PreTokenize(
    absl::string_view text) const {
  return Postprocess(Tokenize(Preprocess(text)));
}

// static
std::string PretokenizerForTrainingInterface::Preprocess(
    absl::string_view text) {
  // Escapes kWSStr (_) as this character may not be processed by pre-tokenizer.
  return absl::StrReplaceAll(text, {{kWSStr, " "}});
}

// static
std::vector<std::string> PretokenizerForTrainingInterface::Postprocess(
    const SentencePieceText& spt) {
  // Inserts kUPPBoundaryStr before/after of token boundaries.
  std::vector<std::string> result;
  std::string output;

  int prev = 0;
  for (const auto &piece : spt.pieces()) {
    if (prev == piece.begin() && piece.begin() != 0) {
      result.push_back(output);
      output.clear();
    } else {
      output.append(piece.begin() - prev, ' ');
    }
    output += piece.surface();
    prev = piece.end();
  }

  if (!output.empty()) {
    result.push_back(output);
  }

  for (auto& w : result) {
    w = absl::StrReplaceAll(w, {{" ", kWSStr}});
  }

  return result;
}

}  // namespace pretokenizer
}  // namespace sentencepiece
