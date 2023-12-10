// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>

#include <cmath>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "dpf/int_mod_n.h"
#include "openssl/rand.h"

namespace distributed_point_functions {
namespace {

using MyInt = IntModN<uint32_t, 4294967291u>;  // 2**32 - 5.
constexpr int kNumSamples = 5;

void BM_Sample(benchmark::State& state) {
  int num_iterations = state.range(0);
  double security_parameter = 40 + std::log2(num_iterations);
  std::vector<uint8_t> bytes(
      MyInt::GetNumBytesRequired(kNumSamples, security_parameter).value());
  RAND_bytes(bytes.data(), bytes.size());
  std::vector<MyInt> output(num_iterations * kNumSamples);
  for (auto s : state) {
    for (int i = 0; i < num_iterations; ++i) {
      MyInt::UnsafeSampleFromBytes<kNumSamples>(
          absl::string_view(reinterpret_cast<const char*>(bytes.data()),
                            bytes.size()),
          security_parameter,
          absl::MakeSpan(&output[i * kNumSamples], kNumSamples));
    }
    benchmark::DoNotOptimize(output);
  }
}
BENCHMARK(BM_Sample)->Range(1, 1 << 20);

}  // namespace
}  // namespace distributed_point_functions
