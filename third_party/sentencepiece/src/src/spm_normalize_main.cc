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

#include <gflags/gflags.h>

#include "src/builder.h"
#include "src/common.h"
#include "src/filesystem.h"
#include "src/normalizer.h"
#include "src/sentencepiece.pb.h"
#include "src/sentencepiece_model.pb.h"
#include "src/sentencepiece_processor.h"
#include "src/sentencepiece_trainer.h"

DEFINE_string(model, "", "Model file name");
DEFINE_bool(use_internal_normalization, false,
            "Use NormalizerSpec \"as-is\" to run the normalizer "
            "for SentencePiece segmentation");
DEFINE_string(normalization_rule_name, "",
              "Normalization rule name. "
              "Choose from nfkc or identity");
DEFINE_string(normalization_rule_tsv, "", "Normalization rule TSV file. ");
DEFINE_bool(remove_extra_whitespaces, true, "Remove extra whitespaces");
DEFINE_bool(decompile, false,
            "Decompile compiled charamap and output it as TSV.");
DEFINE_string(input, "", "Input filename");
DEFINE_string(output, "", "Output filename");

using sentencepiece::ModelProto;
using sentencepiece::NormalizerSpec;
using sentencepiece::SentencePieceProcessor;
using sentencepiece::SentencePieceTrainer;
using sentencepiece::normalizer::Builder;
using sentencepiece::normalizer::Normalizer;

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::string> rest_args;
  if (FLAGS_input.empty()) {
    for (int i = 1; i < argc; ++i) {
      rest_args.push_back(std::string(argv[i]));
    }
  } else {
    rest_args.push_back(FLAGS_input);
  }

  NormalizerSpec spec;

  if (!FLAGS_model.empty()) {
    ModelProto model_proto;
    SentencePieceProcessor sp;
    CHECK_OK(sp.Load(FLAGS_model));
    spec = sp.model_proto().normalizer_spec();
  } else if (!FLAGS_normalization_rule_tsv.empty()) {
    spec.set_normalization_rule_tsv(FLAGS_normalization_rule_tsv);
    CHECK_OK(SentencePieceTrainer::PopulateNormalizerSpec(&spec));
  } else if (!FLAGS_normalization_rule_name.empty()) {
    spec.set_name(FLAGS_normalization_rule_name);
    CHECK_OK(SentencePieceTrainer::PopulateNormalizerSpec(&spec));
  } else {
    LOG(FATAL) << "Sets --model, normalization_rule_tsv, or "
                  "normalization_rule_name flag.";
  }

  // Uses the normalizer spec encoded in the model_pb.
  if (!FLAGS_use_internal_normalization) {
    spec.set_add_dummy_prefix(false);    // do not add dummy prefix.
    spec.set_escape_whitespaces(false);  // do not output meta symbol.
    spec.set_remove_extra_whitespaces(FLAGS_remove_extra_whitespaces);
  }

  if (FLAGS_decompile) {
    Builder::CharsMap chars_map;
    CHECK_OK(
        Builder::DecompileCharsMap(spec.precompiled_charsmap(), &chars_map));
    CHECK_OK(Builder::SaveCharsMap(FLAGS_output, chars_map));
  } else {
    const Normalizer normalizer(spec);
    auto output = sentencepiece::filesystem::NewWritableFile(FLAGS_output);
    CHECK_OK(output->status());

    if (rest_args.empty()) {
      rest_args.push_back("");  // empty means that read from stdin.
    }

    std::string line;
    for (const auto &filename : rest_args) {
      auto input = sentencepiece::filesystem::NewReadableFile(filename);
      CHECK_OK(input->status());
      while (input->ReadLine(&line)) {
        output->WriteLine(normalizer.Normalize(line));
      }
    }
  }

  return 0;
}
