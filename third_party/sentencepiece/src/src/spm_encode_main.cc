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

#include <functional>
#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/common.h"
#include "src/filesystem.h"
#include "src/sentencepiece.pb.h"
#include "src/sentencepiece_processor.h"
#include "src/trainer_interface.h"

DEFINE_string(model, "", "model file name");
DEFINE_string(
    output_format, "piece",
    "choose from piece, id, proto, nbest_piece, nbest_id, or nbest_proto");
DEFINE_string(input, "", "input filename");
DEFINE_string(output, "", "output filename");
DEFINE_string(extra_options, "",
              "':' separated encoder extra options, e.g., \"reverse:bos:eos\"");
DEFINE_int32(nbest_size, 10, "NBest size");
DEFINE_double(alpha, 0.5, "Smoothing parameter for sampling mode.");

// Piece restriction with vocabulary file.
// https://github.com/rsennrich/subword-nmt#best-practice-advice-for-byte-pair-encoding-in-nmt
DEFINE_string(vocabulary, "",
              "Restrict the vocabulary. The encoder only emits the "
              "tokens in \"vocabulary\" file");
DEFINE_int32(vocabulary_threshold, 0,
             "Words with frequency < threshold will be treated as OOV");
DEFINE_bool(generate_vocabulary, false,
            "Generates vocabulary file instead of segmentation");

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

  CHECK(!FLAGS_model.empty());

  sentencepiece::SentencePieceProcessor sp;
  CHECK_OK(sp.Load(FLAGS_model));
  CHECK_OK(sp.SetEncodeExtraOptions(FLAGS_extra_options));

  if (!FLAGS_vocabulary.empty()) {
    CHECK_OK(sp.LoadVocabulary(FLAGS_vocabulary, FLAGS_vocabulary_threshold));
  }

  auto output = sentencepiece::filesystem::NewWritableFile(FLAGS_output);
  CHECK_OK(output->status());

  if (rest_args.empty()) {
    rest_args.push_back("");  // empty means that reading from stdin.
  }

  std::string line;
  std::vector<std::string> sps;
  std::vector<int> ids;
  std::vector<std::vector<std::string>> nbest_sps;
  std::vector<std::vector<int>> nbest_ids;
  absl::flat_hash_map<std::string, int> vocab;
  sentencepiece::SentencePieceText spt;
  sentencepiece::NBestSentencePieceText nbest_spt;
  std::function<void(const std::string &line)> process;

  if (FLAGS_generate_vocabulary) {
    process = [&](const std::string &line) {
      CHECK_OK(sp.Encode(line, &spt));
      for (const auto &piece : spt.pieces()) {
        if (!sp.IsUnknown(piece.id()) && !sp.IsControl(piece.id()))
          vocab[piece.piece()]++;
      }
    };
  } else if (FLAGS_output_format == "piece") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.Encode(line, &sps));
      output->WriteLine(absl::StrJoin(sps, " "));
    };
  } else if (FLAGS_output_format == "id") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.Encode(line, &ids));
      output->WriteLine(absl::StrJoin(ids, " "));
    };
  } else if (FLAGS_output_format == "proto") {
    process = [&](const std::string &line) { CHECK_OK(sp.Encode(line, &spt)); };
  } else if (FLAGS_output_format == "sample_piece") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.SampleEncode(line, FLAGS_nbest_size, FLAGS_alpha, &sps));
      output->WriteLine(absl::StrJoin(sps, " "));
    };
  } else if (FLAGS_output_format == "sample_id") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.SampleEncode(line, FLAGS_nbest_size, FLAGS_alpha, &ids));
      output->WriteLine(absl::StrJoin(ids, " "));
    };
  } else if (FLAGS_output_format == "sample_proto") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.SampleEncode(line, FLAGS_nbest_size, FLAGS_alpha, &spt));
    };
  } else if (FLAGS_output_format == "nbest_piece") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.NBestEncode(line, FLAGS_nbest_size, &nbest_sps));
      for (const auto &result : nbest_sps) {
        output->WriteLine(absl::StrJoin(result, " "));
      }
    };
  } else if (FLAGS_output_format == "nbest_id") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.NBestEncode(line, FLAGS_nbest_size, &nbest_ids));
      for (const auto &result : nbest_ids) {
        output->WriteLine(absl::StrJoin(result, " "));
      }
    };
  } else if (FLAGS_output_format == "nbest_proto") {
    process = [&](const std::string &line) {
      CHECK_OK(sp.NBestEncode(line, FLAGS_nbest_size, &nbest_spt));
    };
  } else {
    LOG(FATAL) << "Unknown output format: " << FLAGS_output_format;
  }

  for (const auto &filename : rest_args) {
    auto input = sentencepiece::filesystem::NewReadableFile(filename);
    CHECK_OK(input->status());
    while (input->ReadLine(&line)) {
      process(line);
    }
  }

  if (FLAGS_generate_vocabulary) {
    for (const auto &it : sentencepiece::Sorted(vocab)) {
      output->WriteLine(it.first + "\t" +
                        sentencepiece::string_util::SimpleItoa(it.second));
    }
  }

  return 0;
}
