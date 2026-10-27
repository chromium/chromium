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

#include <string>
#include <vector>

#include "common.h"
#include "filesystem.h"
#include "model_factory.h"
#include "model_interface.h"
#include "normalizer.h"
#include "sentencepiece.pb.h"
#include "sentencepiece_processor.h"
#include "testharness.h"
#include "absl/log/check.h"
#include "absl/log/flags.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "third_party/benchmark/include/benchmark/benchmark.h"
#include "util.h"

ABSL_FLAG(std::string, test_srcdir, sentencepiece::util::JoinPath("..", "data"),
          "Data directory.");
ABSL_FLAG(std::string, test_tmpdir, "test_tmp", "Temporary directory.");

namespace sentencepiece {

enum class BenchmarkMode {
  kSequential = 0,
  kParallel = 1,
};

// Parameters for benchmarking ParallelEncode. These values come from the
// recommendations in the ParallelEncode documentation.
constexpr int kChunkLength = 10000;
constexpr int kNumThreads = 16;

ModelProto LoadModelProto(absl::string_view filename) {
  auto input = filesystem::NewReadableFile(filename, /*is_binary=*/true);
  ABSL_CHECK_OK(input->status());
  std::string serialized;
  ABSL_CHECK(input->ReadAll(&serialized));
  ModelProto model_proto;
  ABSL_CHECK(model_proto.ParseFromString(serialized));
  return model_proto;
}

std::string LoadInput(absl::string_view filename) {
  auto input = filesystem::NewReadableFile(filename, /*is_binary=*/false);
  ABSL_CHECK_OK(input->status());
  std::string serialized;
  ABSL_CHECK(input->ReadAll(&serialized));
  return serialized;
}

template <BenchmarkMode kMode>
void BM_Encode(benchmark::State& state, absl::string_view model_filename,
               absl::string_view input_filename) {
  const std::string model_fullpath =
      util::JoinPath(testing::SrcDir(), model_filename);
  const ModelProto model_proto = LoadModelProto(model_fullpath);
  SentencePieceProcessor processor;
  ABSL_CHECK_OK(processor.Load(model_proto));

  const std::string input_fullpath =
      util::JoinPath(testing::SrcDir(), input_filename);
  std::string input = LoadInput(input_fullpath);
  std::vector<int> ids;
  ThreadPool thread_pool(kNumThreads);
  for (auto s : state) {
    benchmark::DoNotOptimize(input);

    absl::Status result;
    if constexpr (kMode == BenchmarkMode::kParallel) {
      result = processor.ParallelEncode(input, kChunkLength, thread_pool, &ids);
    } else {
      result = processor.Encode(input, &ids);
    }

    benchmark::DoNotOptimize(result);
  }

  state.SetBytesProcessed(state.iterations() * input.size());
}

// A benchmark that splits the input data by line and runs many more smaller
// encodes. This maybe more akin to ML model training that will encode examples
// that are expected to have anywhere from 1~32K token inputs, with multiple
// sequences in a batch.
void BM_Encode_ShortLines(benchmark::State& state,
                          absl::string_view model_filename,
                          absl::string_view input_filename) {
  const std::string model_fullpath =
      util::JoinPath(testing::SrcDir(), model_filename);
  const ModelProto model_proto = LoadModelProto(model_fullpath);
  SentencePieceProcessor processor;
  ABSL_CHECK_OK(processor.Load(model_proto));

  const std::string input_fullpath =
      util::JoinPath(testing::SrcDir(), input_filename);
  std::string input = LoadInput(input_fullpath);
  std::vector<absl::string_view> lines = absl::StrSplit(input, '\n');
  std::vector<int> ids;
  absl::Status result;
  for (auto s : state) {
    benchmark::DoNotOptimize(lines);
    for (const auto& line : lines) {
      result = processor.Encode(line, &ids);
      benchmark::DoNotOptimize(result);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetBytesProcessed(state.iterations() * input.size());
}

enum class DecodeInputMode {
  kIds = 0,
  kPieces = 1,
};

template <DecodeInputMode kInputMode>
void BM_Decode(benchmark::State& state, absl::string_view model_filename,
               absl::string_view input_filename) {
  const std::string model_fullpath =
      util::JoinPath(testing::SrcDir(), model_filename);
  const ModelProto model_proto = LoadModelProto(model_fullpath);
  SentencePieceProcessor processor;
  CHECK_OK(processor.Load(model_proto));

  const std::string input_fullpath =
      util::JoinPath(testing::SrcDir(), input_filename);
  std::string input = LoadInput(input_fullpath);

  std::vector<int> ids;
  std::vector<std::string> pieces;
  CHECK_OK(processor.Encode(input, &ids));
  CHECK_OK(processor.Encode(input, &pieces));

  std::string detokenized;
  for (auto s : state) {
    benchmark::DoNotOptimize(ids);
    benchmark::DoNotOptimize(pieces);

    absl::Status result;
    if constexpr (kInputMode == DecodeInputMode::kIds) {
      result = processor.Decode(ids, &detokenized);
    } else {
      result = processor.Decode(pieces, &detokenized);
    }

    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(detokenized);
  }

  state.SetBytesProcessed(state.iterations() * input.size());
}

void BM_EncodeBotchan_Sequential(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kSequential>(state, "botchan_1000_bpe.model",
                                        "botchan.txt");
}
BENCHMARK(BM_EncodeBotchan_Sequential);

void BM_EncodeBotchan_Parallel(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kParallel>(state, "botchan_1000_bpe.model",
                                      "botchan.txt");
}
BENCHMARK(BM_EncodeBotchan_Parallel);

void BM_EncodeBotchan_ShortLines(benchmark::State& state) {
  BM_Encode_ShortLines(state, "botchan_1000_bpe.model", "botchan.txt");
}
BENCHMARK(BM_EncodeBotchan_ShortLines);

void BM_EncodeWagahaiwaNekodearu_Sequential(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kSequential>(
      state, "wagahaiwa_nekodearu_2000_bpe_byte.model",
      "wagahaiwa_nekodearu.txt");
}
BENCHMARK(BM_EncodeWagahaiwaNekodearu_Sequential);

void BM_EncodeWagahaiwaNekodearu_Parallel(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kParallel>(state,
                                      "wagahaiwa_nekodearu_2000_bpe_byte.model",
                                      "wagahaiwa_nekodearu.txt");
}
BENCHMARK(BM_EncodeWagahaiwaNekodearu_Parallel);

void BM_OSSModel_Sequential(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kSequential>(state, "test_oss_model.model",
                                        "botchan.txt");
}
BENCHMARK(BM_OSSModel_Sequential);

void BM_OSSModel_Parallel(benchmark::State& state) {
  BM_Encode<BenchmarkMode::kParallel>(state, "test_oss_model.model",
                                      "botchan.txt");
}
BENCHMARK(BM_OSSModel_Parallel);

void BM_DecodeBotchan_Ids(benchmark::State& state) {
  BM_Decode<DecodeInputMode::kIds>(state, "botchan_1000_bpe.model",
                                    "botchan.txt");
}
BENCHMARK(BM_DecodeBotchan_Ids);

void BM_DecodeBotchan_Pieces(benchmark::State& state) {
  BM_Decode<DecodeInputMode::kPieces>(state, "botchan_1000_bpe.model",
                                       "botchan.txt");
}
BENCHMARK(BM_DecodeBotchan_Pieces);

void BM_DecodeWagahaiwaNekodearu_Ids(benchmark::State& state) {
  BM_Decode<DecodeInputMode::kIds>(
      state, "wagahaiwa_nekodearu_2000_bpe_byte.model",
      "wagahaiwa_nekodearu.txt");
}
BENCHMARK(BM_DecodeWagahaiwaNekodearu_Ids);

void BM_DecodeOSSModel_Ids(benchmark::State& state) {
  BM_Decode<DecodeInputMode::kIds>(state, "test_oss_model.model",
                                    "botchan.txt");
}
BENCHMARK(BM_DecodeOSSModel_Ids);

}  // namespace sentencepiece

BENCHMARK_MAIN();
